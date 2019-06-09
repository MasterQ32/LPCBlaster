#include "mainwindow.hpp"
#include "ui_mainwindow.h"

#include "../BlasterFirmware/errorcode.hpp"

#include <QThread>
#include <QMessageBox>
#include <QDebug>
#include <QSerialPortInfo>
#include <QTextCursor>
#include <QTimer>
#include <QFile>

#include <elfloader.hpp>

namespace UU
{
	static char encode_bits(char c)
	{
		assert(c >= 0 and c <= 63);
		if(c == 0)
			return 96;
		else if(c >= 1 and c <= 63)
			return c + 32;
		assert(false);
	}


	static QByteArray encode_line(QByteArray const & src)
	{
		QByteArray line;
		line.append(encode_bits(src.size()));

		for(int i = 0; i < src.size(); i += 3)
		{
			auto const b0 = ((i + 0) < src.size()) ? src[i + 0] : 0;
			auto const b1 = ((i + 1) < src.size()) ? src[i + 1] : 0;
			auto const b2 = ((i + 2) < src.size()) ? src[i + 2] : 0;

			auto const a = ((b0 & 0xFC) >> 2) & 0x3F;
			auto const b = ((b0 & 0x03) << 4) | ((b1 & 0xF0) >> 4);
			auto const c = ((b1 & 0x0F) << 2) | ((b2 & 0xC0) >> 6);
			auto const d = (b2 & 0x3F);

			line.append(encode_bits(char(a)));
			line.append(encode_bits(char(b)));
			line.append(encode_bits(char(c)));
			line.append(encode_bits(char(d)));
		}

		return line;
	}

	static QByteArrayList encode(QByteArray const & src)
	{
		QByteArrayList result;

		for(int offset = 0; offset < src.size(); offset += 45)
		{
			// blocks of 45 length;
			int len = std::min(45, src.size() - offset);

			result.append(encode_line(src.mid(offset, len)));
		}

		result.append(QByteArray(1, encode_bits(0)));

		return result;
	}

	static char decode_bits(char c)
	{
		if(c == 96)
			return 0;
		else if(c >= 33 and c <= 95)
			return c- 32;
		else
			assert(false and "out of range error");
	}

	static int decode_into(QByteArray & dest, QByteArray const & src)
	{
		assert(src.size() > 0);
		assert((src.size() - 1) % 4 == 0);
		auto len = decode_bits(src[0]);
		if(len == 0)
			return 0;
		int start_size = dest.size();
		for(int i = 1; i < src.size(); i += 4)
		{
			auto const a = decode_bits(src[i + 0]);
			auto const b = decode_bits(src[i + 1]);
			auto const c = decode_bits(src[i + 2]);
			auto const d = decode_bits(src[i + 3]);

			auto const b0 = ((a & 0x3F) << 2) | ((b & 0x30) >> 4);
			auto const b1 = ((b & 0x0F) << 4) | ((c & 0x3C) >> 2);
			auto const b2 = ((c & 0x03) << 6) | ((d & 0x3F) >> 0);

			dest.append(char(b0));
			if((--len) == 0)
				break;

			dest.append(char(b1));
			if((--len) == 0)
				break;

			dest.append(char(b2));
			if((--len) == 0)
				break;
		}
		return dest.size() - start_size;
	}

	static QByteArray decode(QByteArrayList const & lines)
	{
		QByteArray result;
		for(auto const & line : lines)
			decode_into(result, line);
		return result;
	}
}

MainWindow::MainWindow(QWidget *parent) :
  QMainWindow(parent),
  ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	stateLabel = new QLabel(this);
	ui->statusBar->addWidget(stateLabel);

	connect(&port, &QSerialPort::readyRead, this, &MainWindow::on_port_ready);
	connect(&port, &QSerialPort::errorOccurred, this, [](QSerialPort::SerialPortError err) {
		qDebug() << "serial port error:" << err;
	});

	// get new port list and refresh the UI
	on_refreshPortListButton_clicked();
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::enableBootloader(bool enabled)
{
	port.setRequestToSend(enabled); // BOOT ENA
}

void MainWindow::enableReset(bool reset)
{
	port.setDataTerminalReady(reset);
}

bool MainWindow::connectToISP()
{
	enableBootloader(true);
	enableReset(true);
	QThread::msleep(100);
	enableReset(false);
	QThread::msleep(100);

	state = WaitForInitialSynchronized;
	port.clear(); // flush FIFOs
	port.write("?");

	return true;
}

void MainWindow::on_connectButton_clicked()
{
	if(not connectToISP())
		QMessageBox::warning(this, this->windowTitle(), "Failed to connect to ISP!");
	updateUI();
}

void MainWindow::on_sendButton_clicked()
{
	port.write(ui->message->text().toUtf8());
	port.write("\r\n");
	updateUI();
}

void MainWindow::on_port_ready()
{
	while(not port.atEnd())
	{
		if(not process_port_data())
			break;
	}
	updateUI();
}

bool MainWindow::process_port_data()
{
	switch(state)
	{
		case WaitForInitialSynchronized:
		{
			if(not port.canReadLine())
				return false;
			auto line = port.readLine();
			if(line == "Synchronized\r\n") {
				port.write("Synchronized\r\n");
				state = WaitForInitialOK;
				qDebug() << "initial synchronize received";
			} else {
				qDebug() << "wfis" << line;
			}

			return true;
		}
		case WaitForInitialOK:
		{
			if(not port.canReadLine())
				return false;
			auto line = port.readLine();
			if(line == "Synchronized\rOK\r\n") {
				port.write("12000\r\n");
				state = WaitForFrequencyOK;
				qDebug() << "initial ok received";
			} else {
				qDebug() << "wfio" << line;
			}

			return true;
		}

		case WaitForFrequencyOK:
		{
			if(not port.canReadLine())
				return false;
			auto line = port.readLine();
			if(line == "12000\rOK\r\n") {
				port.write("A 0\r\n");
				state = WaitForEchoACK;
				qDebug() << "frequency set: ok";
			} else {
				qDebug() << "wffo" << line;
			}
			return true;
		}

		case WaitForEchoACK:
		{
			if(not port.canReadLine())
				return false;
			auto line = port.readLine();
			if(line == "A 0\r0\r\n") {
				qDebug() << "echo disabled, connection established";
				state = ConnectionEstablished;
			} else {
				qDebug() << "wfea" << line;
			}
			return true;
		}

		case PortOpen:
		case ConnectionEstablished:
		case LPCBlasterReady:
			log(port.readAll());
			return true;

		case CommandStarted:
			assert(this->currentCommand);
			return this->currentCommand->onData();

		case LPCBlasterTransfer:
		{
			auto data = port.read(1);
			if(data.isEmpty())
				return false;
			assert(data.size() == 1);
			assert(data[0] == '\006' or data[0] == '\025');
			if(data[0] == '\006') {
				if(readback)
					state = LPCBlasterReadbackData;
				else
					state = LPCBlasterReady;
			}
			else {
				state = LPCBlasterError;
			}
			return true;
		}

		case LPCBlasterError:
		{
			auto data = port.read(1);
			if(data.isEmpty())
				return false;
			assert(data.size() == 1);

			logLine(QString("LPCBlaster returned error: %0").arg(int(data[0])));

			state = LPCBlasterReady;
			return true;
		}

		case LPCBlasterReadbackData:
		{
			assert(readback);
			auto & data = *readback;

			int rest = data.length - data.buffer.size();
			assert(rest > 0);

			auto newData = port.read(rest);
			assert(newData.size() <= rest);
			data.buffer.append(newData);

			data.progress->setValue(100 * data.buffer.size() / data.length);

			qDebug() << "received" << newData.size() << "bytes, summing up to" << data.buffer.size();

			if(data.buffer.size() == data.length)
			{
				dumpHex(data.buffer, data.offset);
				state = LPCBlasterReadbackChecksum;
			}

			return true;
		}

		case LPCBlasterReadbackChecksum:
		{
			assert(readback);
			assert(readback->buffer.size() == readback->length);

			auto cs_data = port.read(2);
			if(cs_data.size() < 2) {
				if(cs_data.size() == 1)
					port.ungetChar(cs_data[0]);
				return false;
			}
			assert(cs_data.size() == 2);

			uint16_t remote_checksum = *reinterpret_cast<uint16_t const *>(cs_data.data());
			uint16_t local_checksum = 0;
			for(uint8_t c : readback->buffer)
				local_checksum += c;

			if(local_checksum != remote_checksum)
				logLine(QString("checksum: bad (%0 != %1)").arg(local_checksum).arg(remote_checksum));
			else
				logLine(QString("checksum: good"));

			state = LPCBlasterReady;
			readback.reset();

			return true;
		}

		default:
			qDebug() << "unknown state" << int(state) << ":" << port.readAll();
			return true;
	}
	assert(false);
}

void MainWindow::updateUI()
{
	bool const isOpen = port.isOpen();

	ui->refreshPortListButton->setEnabled(not isOpen);
	ui->portList->setEnabled(not isOpen);

	ui->openComButton->setEnabled(ui->portList->currentIndex() >= 0);
	ui->openComButton->setText(isOpen ? "Close" :  "Open");

	ui->connectButton->setEnabled(isOpen);
	ui->resetButton->setEnabled(isOpen);

	ui->message->setEnabled(isOpen and (state == ConnectionEstablished or state == PortOpen or state == LPCBlasterReady));
	ui->sendButton->setEnabled(ui->message->isEnabled());

	ui->readBlockButton->setEnabled(isOpen and state == ConnectionEstablished);
	ui->writeBlockButton->setEnabled(isOpen and state == ConnectionEstablished);
	ui->blockData->setEnabled(isOpen and state == ConnectionEstablished);
	ui->startBlasterButton->setEnabled(isOpen and state == ConnectionEstablished);

	ui->blasterTabs->setEnabled(isOpen and state == LPCBlasterReady);

	{
		QString stateText = "Disconnected";
		if(isOpen)
		{
			switch(state)
			{
				case PortOpen:                   stateText = "Port openend"; break;
				case WaitForInitialSynchronized: stateText = "Waiting for synchronization…"; break;
				case WaitForInitialOK:           stateText = "Waiting for ok…"; break;
				case WaitForFrequencyOK:         stateText = "Setting frequency…"; break;
				case WaitForEchoACK:             stateText = "Wait for echo disable…"; break;
				case ConnectionEstablished:      stateText = "Connected"; break;
				case CommandStarted:             stateText = "Executing command…"; break;
				case LPCBlasterReady:            stateText = "LPCBlaster ready"; break;
				case LPCBlasterTransfer:         stateText = "LPCBlaster working…"; break;
				case LPCBlasterError:            stateText = "Waiting for LPCBlaster error…"; break;
				case LPCBlasterReadbackData:     stateText = "LPCBlaster transferring data…"; break;
				case LPCBlasterReadbackChecksum: stateText = "LPCBlaster transferring data…"; break;
			}
		}
		stateLabel->setText(stateText);
	}
}

void MainWindow::dumpHex(const QByteArray & data, int absolute_offset)
{
	for(int offset = 0; offset < data.size(); offset += 16)
	{
		int len = std::min(16, data.size() - offset);

		QString str;
		str += ("00000000" + QString::number(absolute_offset + offset, 16)).right(8) + " ";
		for(int i = offset; i < offset + len; i++)
			str += " " + ("00" + QString::number(data[i], 16)).right(2);
		for(int i = 0; i < 16 - len; i++)
			str += "   ";

		str += "  |";
		for(int i = offset; i < offset + len; i++)
		{
			char c = data[i];
			if(c >= 32 and c <= 126)
				str += QString(1, c);
			else
				str += ".";
		}
		for(int i = 0; i < 16 - len; i++)
			str += " ";
		str += "|";
		logLine(str);
	}
}

void MainWindow::log(const QByteArray & data)
{
	log(QString::fromUtf8(data));
}

void MainWindow::log(const QString & str)
{
	ui->log->moveCursor(QTextCursor::End);
	ui->log->insertPlainText(str);
	ui->log->moveCursor(QTextCursor::End);
}

void MainWindow::logLine(const QByteArray & data)
{
	log(data);
	log(QString("\r\n"));
}

void MainWindow::logLine(const QString & str)
{
	log(str);
	log(QString("\r\n"));
}

void MainWindow::on_resetButton_clicked()
{
	enableBootloader(false);
	enableReset(true);
	QThread::msleep(100);
	enableReset(false);
	state = PortOpen;
	updateUI();
}

void MainWindow::on_encodeTestButton_clicked()
{
}

void MainWindow::on_openComButton_clicked()
{
	if(port.isOpen())
	{
		port.close();
	}
	else
	{
		port.setPortName("/dev/" + ui->portList->currentData().toString());
		bool ok = port.open(QSerialPort::ReadWrite);
		if(ok)
		{
			bool good = true;
			good &= port.setBaudRate(115200);
			good &= port.setDataBits(QSerialPort::Data8);
			good &= port.setStopBits(QSerialPort::OneStop);
			good &= port.setParity(QSerialPort::NoParity);
			good &= port.setFlowControl(QSerialPort::SoftwareControl);

			port.setReadBufferSize(1 << 20); // 1 MB

			assert(good);

			enableBootloader(false);
			enableReset(false);
			port.clear(); // flush FIFOs
			state = PortOpen;
		}
	}
	updateUI();
}

void MainWindow::on_refreshPortListButton_clicked()
{
	ui->portList->clear();
	for(auto const & port : QSerialPortInfo::availablePorts())
	{
		ui->portList->addItem(port.description() + " (" + port.portName() + ")", port.portName());
	}
	updateUI();
}

void MainWindow::on_message_returnPressed()
{
	on_sendButton_clicked();
	ui->message->setText("");
}

struct ReadCommand : MainWindow::Command
{
	enum State { ReadStatus, ReadData, ReadChecksum };

	uint32_t start_offset;
	uint32_t remaining;
	State state = ReadData;
	QByteArray buffer;
	int checksum_offset = 0;
	int line_counter = 0;

	explicit ReadCommand(uint32_t startOffset, uint32_t length) :
	  start_offset(startOffset),
	  remaining(length)
	{
		assert(start_offset % 4 == 0);
		assert(length % 4 == 0);
	}

	void onInit() override
	{
		write(QString("R %0 %1\r\n").arg(start_offset).arg(remaining).toUtf8());
		line_counter = 0;
		state = ReadStatus;
	}

	void processLine()
	{
		auto const line = readLine();
		if(line.isEmpty())
			return;
		switch(state)
		{
			case ReadStatus:
			{
				bool ok;
				int status = line.toInt(&ok);
				assert(ok);

				if(status != 0)
				{
					qDebug() << "Failed to read data:" << status;
					return done();
				}

				qDebug() << "Got OK.";

				state = ReadData;

				break;
			}

			case ReadData:
			{
				int len = UU::decode_into(buffer, line.trimmed());
				assert(len <= remaining);
				remaining -= len;
				line_counter++;
				if(line_counter >= 20 or remaining == 0)
				{
					assert(line_counter <= 20);
					qDebug() << "Read" << line_counter << "lines";
					state = ReadChecksum;
				}
				break;
			}

			case ReadChecksum:
			{
				int cs = 0;
				for(int i = checksum_offset; i < buffer.size(); i++)
					cs += uint8_t(buffer[i]);

				bool ok;
				int cs_syn = line.toInt(&ok);
				assert(ok);

				if(cs_syn != cs)
				{
					qDebug() << "Invalid checksum! Received " << cs_syn << "but i have" << cs;
					remaining += (buffer.size() - checksum_offset);
					buffer.resize(checksum_offset);
					state = ReadData;
					// NAK the checksum
					write("RESEND\r\n");
					break;
				}

				// ACK the checksum
				write("OK\r\n");

				checksum_offset = buffer.size();

				if(remaining > 0) {
					state = ReadData;
					line_counter = 0;
				} else {
					owner->dumpHex(buffer, start_offset);
					return done();
				}
				break;
			}
		}
	}

	bool onData() override
	{
		while(port().canReadLine())
			processLine();
		return false;
	}
};

struct WriteCommand : MainWindow::Command
{
	enum State { ReadStatus, ReadChecksumOK };

	size_t start_offset;
	QByteArray data;
	size_t transferred = 0;
	State state;
	int calculated_checksum;
	int transferred_backup;

	explicit WriteCommand(size_t startOffset, QByteArray const & data) :
	  start_offset(startOffset),
	  data(data)
	{
		assert(start_offset % 4 == 0);
		assert(data.size() % 4 == 0);
	}

	void onInit() override
	{
		port().clear();
		write(QString("W %0 %1\r\n").arg(start_offset).arg(data.size()).toUtf8());
		state = ReadStatus;
	}

	void writeNextBlock()
	{
		transferred_backup = transferred;
		calculated_checksum = 0;
		for(int i = 0; i < 20; i++)
		{
			int remaining = data.size() - transferred;
			if(remaining <= 0)
				break;
			int len = std::min(45, remaining);

			for(int i = transferred; i < transferred + len; i++)
				calculated_checksum += uint8_t(data[i]);

			auto const encoded = UU::encode_line(data.mid(transferred, len));
			assert(encoded.size() <= 61);

			// qDebug() << "> " << encoded << encoded.size();

			write(encoded);
			write("\r\n");
			transferred += len;
		}
		write(QString::number(calculated_checksum).toUtf8());
		write("\r\n");
		state = ReadChecksumOK;
	}

	void processLine()
	{
		auto const line = readLine();
		if(line.isEmpty())
			return;
		qDebug() << "< " << line;
		switch(state)
		{
			case ReadStatus:
			{
				bool ok;
				int status = line.toInt(&ok);
				assert(ok);

				if(status != 0)
				{
					qDebug() << "Failed to read data:" << status;
					return done();
				}

				qDebug() << "Got OK.";

				return writeNextBlock();
			}

			case ReadChecksumOK:
			{
				if(line == "OK\r\n")
				{
					if(transferred == data.size())
						return done();
					else
						return writeNextBlock();
				}
				else if(line == "RESEND\r\n")
				{
					transferred = transferred_backup;
					return writeNextBlock();
				}
				else
				{
					qDebug() << "unkown data:" << line;
				}
			}
		}
	}

	bool onData() override
	{
		while(port().canReadLine())
			processLine();
		return false;
	}
};

struct SimpleCommand : MainWindow::Command
{
	virtual void processLine(QByteArray const & line)
	{
		bool ok;
		int status = line.toInt(&ok);
		assert(ok);

		if(status != 0)
		{
			qDebug() << "Failed to read data:" << status;
			return done();
		}

		qDebug() << "Got OK.";

		return done();
	}

	bool onData() override
	{
		while(port().canReadLine())
		{
			auto const line = readLine();
			if(line.isEmpty())
				continue;
			processLine(line);
			if(this->isDone)
				break;
		}
		return false;
	}
};

struct UnlockCommand : SimpleCommand
{
	void onInit() override
	{
		port().clear();
		write("U 23130\r\n");
	}
};

struct RunCommand : SimpleCommand
{
	enum State { WaitForOK, WaitForBlaster };
	uint32_t start_address;
	State state;

	explicit RunCommand(uint32_t startAddress) :
	  start_address(startAddress)
	{
		assert(start_address % 4 == 0);
	}

	void onInit() override
	{
		port().clear();
		write(QString("G %0 T\r\n").arg(start_address).toUtf8());
		state = WaitForOK;
	}

	void processLine(QByteArray const & line) override
	{
		switch(state)
		{
			case WaitForOK:
			{
				bool ok;
				int status = line.toInt(&ok);
				assert(ok);

				if(status != 0)
				{
					qDebug() << "Failed to read data:" << status;
					return done();
				}

				qDebug() << "Got OK.";
				state = WaitForBlaster;
				break;
			}

			case WaitForBlaster:
			{
				if(line == "LPCBlaster ready.\r\n")
				{
					done();
					assert(owner);
					owner->state = MainWindow::LPCBlasterReady;
					port().setFlowControl(QSerialPort::NoFlowControl);
					return; // everything OK
				}
				break;
			}
		}
	}
};

static uint32_t baseAddress = 0x10001000;

void MainWindow::on_readBlockButton_clicked()
{
	run<ReadCommand>(baseAddress, 512);
}

void MainWindow::on_writeBlockButton_clicked()
{
	int filler = (this->ui->currentFill->intValue() + 1) & 0xFF;
	this->ui->currentFill->display(filler);
/*
	auto const bootloader = ELFLoader::load_binary("BlasterFirmware.bin");
	assert(bootloader);

	QByteArray firmware = std::get<0>(*bootloader);

	firmware.resize(256 * ((firmware.size() + 255) / 256));

	run<WriteCommand>(baseAddress + 0x000, firmware);
	*/
}


void MainWindow::on_startBlasterButton_clicked()
{
	int filler = (this->ui->currentFill->intValue() + 1) & 0xFF;
	this->ui->currentFill->display(filler);

	auto const bootloader = ELFLoader::load_binary("BlasterFirmware.bin");
	assert(bootloader);

	QByteArray firmware = std::get<0>(*bootloader);

	firmware.resize(256 * ((firmware.size() + 255) / 256));

	run<WriteCommand>(baseAddress + 0x000, firmware)
		.continueWith<UnlockCommand>()
		.continueWith<RunCommand>(std::get<1>(*bootloader) & ~1U) // LPCBlasterEntry
	;
}

void MainWindow::run(std::unique_ptr<Command> && command)
{
	assert(state == ConnectionEstablished);
	this->currentCommand = std::move(command);
	this->currentCommand->owner = this;
	state = CommandStarted;
	this->currentCommand->onInit();
}

void MainWindow::Command::done()
{
	assert(owner);
	assert(owner->state == CommandStarted);
	owner->state = ConnectionEstablished;
	isDone = true;

	if(this->continuation)
	{
		auto * win = owner;
		QTimer::singleShot(0, [win]() {
			assert(win->currentCommand);
			assert(win->currentCommand->continuation);
			qDebug() << "continue with new command...";
			auto next = std::move(win->currentCommand->continuation);
			win->run(std::move(next));
		});
	}
}

void MainWindow::Command::write(const QByteArray & data)
{
	qDebug() << "→ " << data;
	port().write(data);
}

QByteArray MainWindow::Command::readLine()
{
	auto data = port().readLine();
	qDebug() << "← " << data;
	return data;
}

void MainWindow::on_clearButton_clicked()
{
	ui->log->setPlainText("");
}

void MainWindow::on_blastReadBackButton_clicked()
{
	bool ok;
	uint32_t offset = ui->blastReadbackMemoryStart->text().toInt(&ok, 16);
	if(not ok)
		return;
	uint32_t length = ui->blastReadbackMemoryLen->text().toInt(&ok, 16);
	if(not ok)
		return;
	port.write("R");
	port.write(reinterpret_cast<char const *>(&offset), 4);
	port.write(reinterpret_cast<char const *>(&length), 4);
	state = LPCBlasterTransfer;
	readback = ReadbackData { offset, length, QByteArray {}, std::make_unique<QProgressBar>() };

	ui->statusBar->addPermanentWidget(readback->progress.get());

	updateUI();
}

void MainWindow::on_blastZeroMemoryButton_clicked()
{
	bool ok;
	uint16_t offset = ui->blastZeroMemoryStart->text().toInt(&ok, 16);
	if(not ok)
		return;
	uint16_t length = ui->blastZeroMemoryLen->text().toInt(&ok, 16);
	if(not ok)
		return;
	port.write("Z");
	port.write(reinterpret_cast<char const *>(&offset), 2);
	port.write(reinterpret_cast<char const *>(&length), 2);
	state = LPCBlasterTransfer;
	updateUI();
}

void MainWindow::on_blastLoadMemoryButton_clicked()
{
	bool ok;
	uint16_t offset = ui->blastZeroMemoryStart->text().toInt(&ok, 16);
	if(not ok)
		return;
	uint16_t length = ui->blastZeroMemoryLen->text().toInt(&ok, 16);
	if(not ok)
		return;
	port.write("L");
	port.write(reinterpret_cast<char const *>(&offset), 2);
	port.write(reinterpret_cast<char const *>(&length), 2);

	QByteArray payload(length, ui->blastLoadMemoryValue->value());
	port.write(payload);

	uint16_t cs = 0;
	for(uint8_t v : payload) cs += v;
	port.write(reinterpret_cast<char const *>(&cs), 2);

	state = LPCBlasterTransfer;
	updateUI();
}
