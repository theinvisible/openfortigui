#include <iostream>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtCore/QDebug>
#include <QtCore/QDir>

#include <QMessageBox>

#include "krunner_openfortigui.h"

#define KB_ASSERT(cond) {if(!(cond)) {qDebug().nospace() << "[" << script << "] Failed on " << #cond; return;}}
#define KB_ASSERT_MSG(cond, msg) {if(!(cond)) {qDebug().nospace() << "[" << script << "] " << msg; return;}}

Krunner_openfortigui::Krunner_openfortigui(QObject* parent, const QVariantList& args)
  : Plasma::AbstractRunner(parent, args)
{
    setSpeed(AbstractRunner::NormalSpeed);
    setPriority(HighestPriority);
    setHasRunOptions(true);

    setDefaultSyntax(Plasma::RunnerSyntax(QString::fromLatin1(":q:"), metadata().comment()));
}

void Krunner_openfortigui::match(Plasma::RunnerContext& ctxt)
{
    if (!ctxt.isValid())
        return;

    QString query = ctxt.query();

    Plasma::QueryMatch match(this);
    match.setText("You typed this: " + query);
    match.setData(query);
    match.setType(Plasma::QueryMatch::CompletionMatch);
    match.setRelevance(1.0);
    match.setMatchCategory("VPN");
    match.setIconName("openfortigui");

    ctxt.addMatch(match);
}

void Krunner_openfortigui::run(const Plasma::RunnerContext& ctxt, const Plasma::QueryMatch& match)
{
    Q_UNUSED(ctxt)

    QMessageBox box;
    box.setText( QString("You typed this: ") + match.data().toString());
    box.setWindowTitle(QString("You typed something"));
    box.exec();
}

K_EXPORT_PLASMA_RUNNER(krunner_openfortigui, Krunner_openfortigui)

#include "krunner_openfortigui.moc"
