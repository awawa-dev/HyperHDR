#pragma once

#include <QString>
#include <QByteArray>

namespace Process {

void restartHyperhdr(bool asNewProcess=false);
QByteArray command_exec(const QString& cmd, const QByteArray& data = {});

}
