#ifndef NATSCLIENT_H
#define NATSCLIENT_H

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QProcessEnvironment>
#include <QStringBuilder>
#include <QUuid>
#include <QTextCodec>
#include <QWebSocket>
#include <memory>

namespace Nats
{
#define DEBUG(x) do { if (_debug_mode) { qDebug() << x; } } while (0)

//! main callback message
using MessageCallback = std::function<void(QString &&message, QString &&inbox, QString &&subject)>;
using ConnectCallback = std::function<void()>;

//!
//! \brief The Options struct
//! holds all client options
struct Options
{
    bool verbose = false;
    bool pedantic = false;
    bool ssl_required = false;
    bool ssl = false;
    bool ssl_verify = true;
    QString ssl_key;
    QString ssl_cert;
    QString ssl_ca;
    QString name = "qt-nats";
    const QString lang = "cpp";
    const QString version = "1.0.0";
    QString user;
    QString pass;
    QString token;
};

//!
//! \brief The Subscription class
//! holds subscription data and emits signal when ready as alternative to callbacks
class Subscription : public QObject
{
    Q_OBJECT

public:
    explicit Subscription(QObject *parent = nullptr): QObject(parent) {}

    QString subject;
    QString message;
    QString inbox;
    uint64_t ssid = 0;
signals:

    void received();
};

//!
//! \brief The Client class
//! main client class
class Client : public QObject
{
    Q_OBJECT
public:

    explicit Client(QObject *parent = nullptr);

    //!
    //! \brief publish
    //! \param subject
    //! \param message
    //! publish given message with subject
    void publish(const QString &subject, const QString &message, const QString &inbox);
    void publish(const QString &subject, const QString &message = "");

    //!
    //! \brief subscribe
    //! \param subject
    //! \param callback
    //! \return subscription id
    //! subscribe to given subject
    //! when message is received, callback is fired
    uint64_t subscribe(const QString &subject, MessageCallback callback);

    //!
    //! \brief subscribe
    //! \param subject
    //! \param queue
    //! \param callback
    //! \return subscription id
    //! overloaded function
    //! subscribe to given subject and queue
    //! when message is received, callback is fired
    //! each message will be delivered to only one subscriber per queue group
    uint64_t subscribe(const QString &subject, const QString &queue, MessageCallback callback);

    //!
    //! \brief subscribe
    //! \param subject
    //! \return
    //! return subscription class holding result for signal/slot version
    Subscription *subscribe(const QString &subject);

    //!
    //! \brief unsubscribe
    //! \param ssid
    //! \param max_messages
    //!
    void unsubscribe(uint64_t ssid, int max_messages = 0);

    //!
    //! \brief request
    //! \param subject
    //! \param message
    //! \return
    //! make request using given subject and optional message
    uint64_t request(const QString subject, const QString message, MessageCallback callback);
    uint64_t request(const QString subject, MessageCallback callback);

signals:

    //!
    //! \brief connected
    //! signal that the client is connected
    void connected();

    //!
    //! \brief connected
    //! signal that the client is connected
    void error(const QString);

    //!
    //! \brief disconnected
    //! signal that the client is disconnected
    void disconnected();

public slots:

    //!
    //! \brief connect
    //! \param host
    //! \param port
    //! connect to server with given host and port options
    //! after valid connection is established 'connected' signal is emitted
    void connect(const QString &host = "127.0.0.1", quint16 port = 9222, ConnectCallback callback = nullptr);
    void connect(const QString &host, quint16 port, const Options &options, ConnectCallback callback = nullptr);

    //!
    //! \brief disconnect
    //! disconnect from server by closing socket
    void disconnect();

private:

    //!
    //! \brief debug_mode
    //! extra debug output, can be set with enviroment variable DEBUG=qt-nats
    bool _debug_mode = true;

    //!
    //! \brief CLRF
    //! NATS protocol separator
    const QString CLRF = "\r\n";

    //!
    //! \brief m_buffer
    //! main buffer that holds data from server
    QByteArray m_buffer;

    //!
    //! \brief m_ssid
    //! subscription id holder
    uint64_t m_ssid = 0;

    //!
    //! \brief m_websocket
    //! main tcp websocket
    QWebSocket m_websocket;

    //!
    //! \brief m_options
    //! client options
    Options m_options;

    //!
    //! \brief m_callbacks
    //! subscription callbacks
    QHash<uint64_t, MessageCallback> m_callbacks;

    //!
    //! \brief send_info
    //! \param options
    //! send client information and options to server
    void send_info(const Options &options);

    //!
    //! \brief parse_info
    //! \param message
    //! \return
    //! parse INFO message from server and return json object with data
    QJsonObject parse_info(const QByteArray &message);

    //!
    //! \brief set_listeners
    //! set connection listeners
    void set_listeners();

    //!
    //! \brief process_inboud
    //! \param buffer
    //! process messages from buffer
    bool process_inbound(const QByteArray &buffer);
};

inline Client::Client(QObject *parent) : QObject(parent)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    _debug_mode = (env.value(QStringLiteral("DEBUG")).indexOf("qt-nats") != -1);

    if(_debug_mode)
        DEBUG("debug mode");
}

inline void Client::connect(const QString &host, quint16 port, ConnectCallback callback)
{
    connect(host, port, m_options, callback);
}

inline void Client::connect(const QString &host, quint16 port, const Options &options, ConnectCallback callback)
{

    // Check is client socket is already connected and return if it is
    if (m_websocket.isValid())
        return;

    QString fullHost = QStringLiteral("wss://") % host % ":" % QString::number(port);


    // When receiving a QWebSocket socketError, print to Debug and emit the error.
    QObject::connect(&m_websocket,QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
                     [=](QAbstractSocket::SocketError socketError)
    {
        DEBUG(socketError);
        emit error(m_websocket.errorString());
    });

    // When a QWebSocket emits a disconnected signal, print to debug, emit a disconnected signal from the client, and disconnect the qwebsocket from everything.
    QObject::connect(&m_websocket, &QWebSocket::disconnected, [this]()
    {
        DEBUG("socket disconnected");
        emit disconnected();

        // Disconnect everything connected to an m_socket's signals
        QObject::disconnect(&m_websocket, nullptr, nullptr, nullptr);
    });

    // receive first info message and disconnect
    auto signal = std::make_shared<QMetaObject::Connection>();
    *signal = QObject::connect(&m_websocket, &QWebSocket::binaryMessageReceived, [this, signal, options, callback](const QByteArray &message)
    {
        QObject::disconnect(*signal);
        QByteArray info_message = message;

        QJsonObject json = parse_info(info_message);

        send_info(options);
        set_listeners();

        if(callback)
            callback();

        emit connected();
    });

    DEBUG("connect started" << host << port);
    m_websocket.open(QUrl(fullHost));
}

inline void Client::disconnect()
{
    m_websocket.flush();
    m_websocket.close();
}

inline void Client::send_info(const Options &options)
{
    QString message =
            QString("CONNECT {")
            % "\"verbose\":" % (options.verbose ? "true" : "false") % ","
            % "\"pedantic\":" % (options.pedantic ? "true" : "false") % ","
            % "\"ssl_required\":" % (options.ssl_required ? "true" : "false") % ","
            % "\"name\":" % "\"" % options.name % "\","
            % "\"lang\":" % "\"" %options.lang % "\","
            % "\"user\":" % "\"" % options.user % "\","
            % "\"pass\":" % "\"" % options.pass % "\","
            % "\"auth_token\":" % "\"" % options.token % "\""
            % "} " % CLRF;

    DEBUG("send info message:" << message);

    m_websocket.sendBinaryMessage(message.toUtf8());
}

inline QJsonObject Client::parse_info(const QByteArray &message)
{
    DEBUG(message);

    // discard 'INFO '
    return QJsonDocument::fromJson(message.mid(5)).object();
}

inline void Client::publish(const QString &subject, const QString &message)
{
    publish(subject, message, "");
}

inline void Client::publish(const QString &subject, const QString &message, const QString &inbox)
{
    QString body = QStringLiteral("PUB ") % subject % " " % inbox % (inbox.isEmpty() ? "" : " ") % QString::number(message.toUtf8().length()) % CLRF % message % CLRF;

    DEBUG("published:" << body);

    m_websocket.sendBinaryMessage(body.toUtf8());
}

inline uint64_t Client::subscribe(const QString &subject, MessageCallback callback)
{
    return subscribe(subject, "", callback);
}

inline uint64_t Client::subscribe(const QString &subject, const QString &queue, MessageCallback callback)
{
    m_callbacks[++m_ssid] = callback;

    QString message = QStringLiteral("SUB ") % subject % " " % queue % (queue.isEmpty() ? "" : " ") % QString::number(m_ssid) % CLRF;

    m_websocket.sendBinaryMessage(message.toUtf8());

    DEBUG("subscribed:" << message);

    return m_ssid;
}

inline Subscription *Client::subscribe(const QString &subject)
{
    auto subscription = new Subscription;

    subscription->ssid = subscribe(subject, "", [subscription](const QString &message, const QString &subject, const QString &inbox)
    {
        subscription->message = message;
        subscription->subject = subject;
        subscription->inbox = inbox;

        emit subscription->received();
    });

    return subscription;
}

inline void Client::unsubscribe(uint64_t ssid, int max_messages)
{
    QString message = QStringLiteral("UNSUB ") % QString::number(ssid) % (max_messages > 0 ? QString(" %1").arg(max_messages) : "") % CLRF;

    DEBUG("unsubscribed:" << message);

    m_websocket.sendBinaryMessage(message.toUtf8());
}

inline uint64_t Client::request(const QString subject, MessageCallback callback)
{
    return request(subject, "", callback);
}

inline uint64_t Client::request(const QString subject, const QString message, MessageCallback callback)
{
    QString inbox = QUuid::createUuid().toString();
    uint64_t ssid = subscribe(inbox, callback);
    unsubscribe(ssid, 1);
    publish(subject, message, inbox);

    return ssid;
}

//! TODO: disconnect handling
inline void Client::set_listeners()
{
    DEBUG("set listeners");
    QObject::connect(&m_websocket, &QWebSocket::binaryMessageReceived, [this](const QByteArray &message)
    {

        // add new data to buffer
        m_buffer +=  message;

        // process message if exists
        int clrf_pos = m_buffer.lastIndexOf(CLRF);
        if(clrf_pos != -1)
        {
            QByteArray msg_buffer = m_buffer.left(clrf_pos + CLRF.length());
            process_inbound(msg_buffer);
        }
    });
}

// parse incoming messages, see http://nats.io/documentation/internals/nats-protocol
// QStringRef is used so we don't do any allocation
// TODO: error on invalid message
inline bool Client::process_inbound(const QByteArray &buffer)
{
    DEBUG("handle message:" << buffer);

    // track movement inside buffer for parsing
    int last_pos = 0, current_pos = 0;

    while(last_pos != buffer.length())
    {
        // we always get delimited message
        current_pos = buffer.indexOf(CLRF, last_pos);
        if(current_pos == -1)
        {
            qCritical() << "CLRF not found, should not happen";
            break;
        }

        QString operation(buffer.mid(last_pos, current_pos - last_pos));

        // if this is PING operation, reply
        if(operation.compare(QStringLiteral("PING"), Qt::CaseInsensitive) == 0)
        {
            DEBUG("sending pong");
            m_websocket.sendBinaryMessage(QString("PONG" % CLRF).toUtf8());
            last_pos = current_pos + CLRF.length();
            continue;
        }
        // +OK operation
        else if(operation.compare(QStringLiteral("+OK"), Qt::CaseInsensitive) == 0)
        {
            DEBUG("+OK");
            last_pos = current_pos + CLRF.length();
            continue;
        }
        // if -ERR, close client connection | -ERR <error message>
        else if(operation.indexOf("-ERR", 0, Qt::CaseInsensitive) != -1)
        {
            QStringRef error_message = operation.midRef(4);

            qCritical() << "error" << error_message;

            if(error_message.compare(QStringLiteral("Invalid Subject")) != 0)
                m_websocket.close();

            return false;
        }
        // only MSG should be now left
        else if(operation.indexOf(QStringLiteral("MSG"), Qt::CaseInsensitive) != 0)
        {
            qCritical() << "invalid message - no message left";

            m_buffer.remove(0,current_pos + CLRF.length());
            return false;
        }

        // extract MSG data
        // MSG format is: 'MSG <subject> <sid> [reply-to] <#bytes>\r\n[payload]\r\n'
        // extract message_len = bytes and check if there is a message in this buffer
        // if not, wait for next call, otherwise, extract all data

        int message_len = 0;
        QStringRef subject, sid, inbox;

        QStringList parts = operation.split(" ", QString::SkipEmptyParts);

        current_pos += CLRF.length();
        if(parts.length() == 4)
        {
            message_len = parts[3].toInt();
        }
        else if (parts.length() == 5)
        {
            inbox = &(parts[3]);
            message_len = parts[4].toInt();
        }
        else
        {
            qCritical() <<  "invalid message - wrong length" << parts.length();
            break;
        }

        if(current_pos + message_len + CLRF.length() > buffer.length())
        {
            DEBUG("message not in buffer, waiting");
            break;
        }

        operation = parts[0];
        subject = &(parts[1]);
        sid = &(parts[2]);
        uint64_t ssid = sid.toULong();

        QString message(buffer.mid(current_pos, message_len));
        last_pos = current_pos + message_len + CLRF.length();

        DEBUG("message:" << message);

        // call correct subscription callback
        if(m_callbacks.contains(ssid))
            m_callbacks[ssid](QString(message), inbox.toString(), subject.toString());
        else
            qWarning() << "invalid callback";
    }

    // remove processed messages from buffer
    m_buffer.remove(0, last_pos);

    return true;
}
}

#endif // NATSCLIENT_H

