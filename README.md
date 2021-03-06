# qt-nats

This is a fork of the original qt-nats that uses QWebSockets instead of QSslSockets.

A [Qt5](https://www.qt.io) C++11 client for the [NATS messaging system](https://nats.io).

[![License MIT](https://cdn.rawgit.com/pkoretic/qt-nats/badges/license.svg)](http://opensource.org/licenses/MIT)
[![Language (C++)](https://cdn.rawgit.com/pkoretic/qt-nats/badges/powered_by-C%2B%2B-blue.svg)](http://en.cppreference.com/w/cpp/language)

## Installation

This is a header-only library that depends on Qt5. All you have to do is include it inside your
project. It depends on the Qt `network` module so don't forget to add it to your `project-name.pro` file (```QT += network```).

For more information see **[examples](examples)**.

```
#include <QCoreApplication>
#include "natsclient.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Nats::Client client;
    client.connect("127.0.0.1", 4222, [&]
    {
        // simple subscribe
        client.subscribe("foo", [](QString message, QString inbox, QString subject)
        {
            qDebug() << "received: " << message << inbox << subject;
        });

        // simple publish
        client.publish("foo", "Hello NATS!");
    });

    return a.exec();
}
```

## Basic usage

```
Nats::Client client;
client.connect("127.0.0.1", 4222, [&]
{
    // simple publish
    client.publish("foo", "Hello World!");

    // simple subscribe
    client.subscribe("foo", [](QString message, QString /* inbox */, QString /* subject */)
    {
        qDebug() << "received message: " << message;
    });

    // unsubscribe
    int sid = client.subscribe("foo", [](QString, QString, QString){});
    client.unsubscribe(sid);

    // request
    client.request("help", [&](QString /* message */, QString reply_inbox, QString /* subject */)
    {
        client.publish(reply_inbox, "I can help");
    });
});
```


## Queue Groups

All subscriptions with the same queue name will form a queue group.  Each
message will be delivered to only one subscriber per queue group, queuing
semantics. You can have as many queue groups as you wish.  Normal subscribers
will continue to work as expected.

```
client.subscribe("foo", "job.workers", [](QString message, QString reply_inbox, QString subject)
{
    qDebug().noquote() << "received message:" << message << subject << reply_inbox;
});
```

## Authentication

```
Nats::Client client;
Nats::Options options;

// username/password
options.user = "user";
options.pass = "pass";

client.connect("127.0.0.1", 4222, options, []
{
    ...
});

// token
options.token = "mytoken";

client.connect("127.0.0.1", 4222, options, []
{
    ...
});
```
## Qt signals

This is Qt specific. If you are used to using Qt signals & slots or you just prefer them over callbacks:

```
Nats::Client client;

QObject::connect(&client, &Nats::Client::connected, [&client]
{
    Nats::Subscription *s = client.subscribe("foo");
    QObject::connect(s, &Nats::Subscription::received, [s]
    {
        qDebug().noquote() << "received message:" << s->message << s->subject << s->inbox;
        s->deleteLater();
    });

    client.publish("foo", "Hello NATS!");
});

client.connect("127.0.0.1", 4222);
```

## Errors and signals

Catch errors:
```
QObject::connect(&client, &Nats::Client::error, [](const QString &error)
{
    qDebug() << error;
});

```

Catch connection disconnect:
```
QObject::connect(&client, &Nats::Client::disconnected, []
{
    qDebug() << "disconnected";
});

```

## Debug mode

For extra debug information env variable can be used:

```
export DEBUG=qt-nats
```
or

```
DEBUG=qt-nats ./program
```
