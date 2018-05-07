#ifndef KRUNNER_OPENFORTIGUI_H
#define KRUNNER_OPENFORTIGUI_H

#include <QtCore/QProcess>
#include <KF5/KRunner/krunner/abstractrunner.h>

class Krunner_openfortigui : public Plasma::AbstractRunner {
    Q_OBJECT

public:
    Krunner_openfortigui(QObject* parent, const QVariantList& args);

    void match(Plasma::RunnerContext&);
    void run(const Plasma::RunnerContext&, const Plasma::QueryMatch&);
};

#endif // KRUNNER_OPENFORTIGUI_H
