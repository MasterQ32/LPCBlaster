#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QSerialPort>
#include <QLabel>
#include <functional>
#include <memory>
#include <QProgressBar>

namespace Ui {
	class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	struct Command
	{
		MainWindow * owner;
		std::unique_ptr<Command> continuation;
		bool isDone = false;

		virtual ~Command() = default;

		virtual void onInit() = 0;
		virtual bool onData() = 0;

		void done();

		QSerialPort & port() {
			assert(owner);
			return owner->port;
		}

		void continueWith(std::unique_ptr<Command> && cmd) {
			continuation = std::move(cmd);
		}

		template<typename T, typename ... Args>
		T & continueWith(Args && ... args)
		{
			continueWith(std::make_unique<T>(std::forward<Args>(args)...));
			return *reinterpret_cast<T*>(continuation.get()) ;
		}

		void write(QByteArray const & data);

		QByteArray readLine();

	};
public:

	enum State {
		PortOpen,
		WaitForInitialSynchronized,
		WaitForInitialOK,
		WaitForFrequencyOK,
		WaitForEchoACK,
		ConnectionEstablished,
		CommandStarted,
		LPCBlasterReady,
		LPCBlasterTransfer,
		LPCBlasterError,
		LPCBlasterReadbackData,
		LPCBlasterReadbackChecksum,
	};

	QSerialPort port;
	State state;
	QLabel * stateLabel;
	std::unique_ptr<Command> currentCommand;

	struct ReadbackData
	{
		uint32_t offset;
		uint32_t length;
		QByteArray buffer;
		std::unique_ptr<QProgressBar> progress;
	};

	std::optional<ReadbackData> readback;

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

public:
	void run(std::unique_ptr<Command> && command);

	template<typename T, typename ... Args>
	T & run(Args && ... args)
	{
		run(std::make_unique<T>(std::forward<Args>(args)...));
		return *reinterpret_cast<T*>(currentCommand.get()) ;
	}

public:
	void dumpHex(QByteArray const & data, int offset = 0);

	void log(QByteArray const & data);

	void log(QString const & str);

	void logLine(QByteArray const & data);

	void logLine(QString const & str);

private:
	void enableBootloader(bool enabled);
	void enableReset(bool reset);

	bool connectToISP();

	void on_port_ready();

	bool process_port_data();

	void updateUI();

private slots:
	void on_connectButton_clicked();

	void on_sendButton_clicked();

	void on_resetButton_clicked();

	void on_encodeTestButton_clicked();

	void on_openComButton_clicked();

	void on_refreshPortListButton_clicked();

	void on_message_returnPressed();

	void on_readBlockButton_clicked();

	void on_writeBlockButton_clicked();

	void on_clearButton_clicked();

	void on_startBlasterButton_clicked();

	void on_blastReadBackButton_clicked();

	void on_blastZeroMemoryButton_clicked();

	void on_blastLoadMemoryButton_clicked();

private:
	Ui::MainWindow *ui;
};

#endif // MAINWINDOW_HPP
