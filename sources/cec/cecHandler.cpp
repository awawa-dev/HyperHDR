#include <cec/cecHandler.h>
#include <utils/Logger.h>

#include <QSocketNotifier>

#include <linux/cec.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

cecHandler::cecHandler() :
	_log("CEC"),
	_fd(-1),
	_notifier(nullptr)
{
	Info(_log, "CEC object created");
}

cecHandler::~cecHandler()
{
	stop();
}

void cecHandler::sendActiveSource()
{
	if (_fd < 0)
		return;

	__u16 phys_addr = CEC_PHYS_ADDR_INVALID;
	if (ioctl(_fd, CEC_ADAP_G_PHYS_ADDR, &phys_addr) < 0
		|| phys_addr == CEC_PHYS_ADDR_INVALID
		|| phys_addr == 0xffff)
	{
		Debug(_log, "Skip ACTIVE_SOURCE – no valid PA");
		return;
	}

	struct cec_log_addrs las = {};
	if (ioctl(_fd, CEC_ADAP_G_LOG_ADDRS, &las) < 0 || las.num_log_addrs < 1)
	{
		Debug(_log, "Skip ACTIVE_SOURCE – no logical address");
		return;
	}

	__u8 la = las.log_addr[0];
	if (la == CEC_LOG_ADDR_INVALID)
	{
		Debug(_log, "Skip ACTIVE_SOURCE – invalid LA");
		return;
	}

	struct cec_msg msg = {};
	msg.msg[0] = (la << 4) | 0xf;
	msg.msg[1] = CEC_MSG_ACTIVE_SOURCE;
	msg.msg[2] = (phys_addr >> 8) & 0xff;
	msg.msg[3] = phys_addr & 0xff;
	msg.len = 4;

	if (ioctl(_fd, CEC_TRANSMIT, &msg) < 0)
		Warning(_log, "ACTIVE_SOURCE failed: {:s} ({:d})", strerror(errno), errno);
	else
		Info(_log, "Sent ACTIVE_SOURCE from LA {} PA={:04x}", la, phys_addr);
}

bool cecHandler::start()
{
	if (_fd >= 0)
		return true;

	Info(_log, "Starting CEC handler (Linux kernel API).");

	for (int i = 0; i < 4; ++i)
	{
		QString devPath = QString("/dev/cec%1").arg(i);
		_fd = ::open(devPath.toUtf8().constData(), O_RDWR | O_NONBLOCK);
		if (_fd >= 0)
		{
			Info(_log, "Opened CEC device: {:s}", devPath);
			break;
		}
	}
	if (_fd < 0)
	{
		Error(_log, "Failed to open any /dev/cec*");
		return false;
	}

	static constexpr __u32 CEC_HYPERHDR_VENDOR_ID_HDMI = 0x000c03;
	struct cec_log_addrs las = {};
	las.cec_version = CEC_OP_CEC_VERSION_2_0;
	las.vendor_id = CEC_HYPERHDR_VENDOR_ID_HDMI;
	las.num_log_addrs = 1;
	las.log_addr_type[0] = CEC_LOG_ADDR_TYPE_PLAYBACK;
	las.primary_device_type[0] = CEC_OP_PRIM_DEVTYPE_PLAYBACK;
	las.all_device_types[0] = CEC_OP_ALL_DEVTYPE_PLAYBACK;
	las.flags = CEC_LOG_ADDRS_FL_ALLOW_UNREG_FALLBACK;
	strncpy((char*)las.osd_name, "HyperHDR", sizeof(las.osd_name) - 1);

	if (ioctl(_fd, CEC_ADAP_S_LOG_ADDRS, &las) < 0)
	{
		Warning(_log, "CEC_ADAP_S_LOG_ADDRS failed: {:s} (adapter already taken?)", strerror(errno));
	}
	else
	{
		Info(_log, "Claimed logical address (Playback)");

		sendActiveSource();
	}

	__u32 mode = CEC_MODE_FOLLOWER | CEC_MODE_INITIATOR;
	if (ioctl(_fd, CEC_S_MODE, &mode) < 0)
	{
		Error(_log, "Failed to set CEC follower mode: {:s} ({:d})", strerror(errno), errno);
		stop();
		return false;
	}

	_notifier = new QSocketNotifier(_fd, QSocketNotifier::Read, this);
	connect(_notifier, &QSocketNotifier::activated, this, &cecHandler::handleMessages);

	return true;
}

void cecHandler::stop()
{
	if (_fd >= 0)
	{
		Info(_log, "Stopping CEC handler");
		if (_notifier)
		{
			_notifier->setEnabled(false);
			_notifier->deleteLater();
			_notifier = nullptr;
		}

		struct cec_log_addrs las = {};
		las.num_log_addrs = 0;
		if (ioctl(_fd, CEC_ADAP_S_LOG_ADDRS, &las) < 0) {
			Debug(_log, "Failed to clear LOG_ADDRS: {:s} ({:d})", strerror(errno), errno);
		}

		close(_fd);
		_fd = -1;
	}
}

void cecHandler::handleMessages()
{
	struct cec_msg msg = {};

	while (_fd >= 0)
	{
		struct cec_event ev = {};
		if (ioctl(_fd, CEC_DQEVENT, &ev) == 0)
		{
			if (ev.event == CEC_EVENT_STATE_CHANGE)
			{
				bool valid = (ev.state_change.phys_addr != CEC_PHYS_ADDR_INVALID);
				Info(_log, "CEC State Change: {:s}, PA={:x}", ((valid) ? "valid" : "invalid"), ev.state_change.phys_addr);
				if (valid) {
					sendActiveSource();
				}
			}
			continue;
		}

		msg = {};
		if (ioctl(_fd, CEC_RECEIVE, &msg) != 0)
		{
			if (errno == EAGAIN)
				break;
			Debug(_log, "CEC_RECEIVE failed: {:s}", strerror(errno));
			break;
		}

		if (!(msg.rx_status & CEC_RX_STATUS_OK) || msg.len < 1)
			continue;

		uint8_t initiator = msg.msg[0] >> 4;
		uint8_t opcode = (msg.len > 1) ? msg.msg[1] : 0;

		switch (opcode)
		{
			case CEC_MSG_STANDBY:
				Info(_log, "CEC standby (OFF)");
				emit stateChange(false, QString::number(initiator));
				break;

			case CEC_MSG_SET_STREAM_PATH:
				Info(_log, "CEC set stream (ON)");
				emit stateChange(true, QString::number(initiator));
				sendActiveSource();
				break;

			case CEC_MSG_USER_CONTROL_PRESSED:
				if (msg.len > 2)
				{
					int key = msg.msg[2];
					Debug(_log, "CEC key pressed: {:d}", key);
					emit keyPressed(key);
				}
				break;

			default:
				break;
		}
	}
}
