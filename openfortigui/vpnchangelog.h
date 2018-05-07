#ifndef VPNCHANGELOG_H
#define VPNCHANGELOG_H

#include <QWidget>

namespace Ui {
class vpnChangelog;
}

class vpnChangelog : public QWidget
{
    Q_OBJECT

public:
    explicit vpnChangelog(QWidget *parent = 0);
    ~vpnChangelog();

    void initAfter();

private:
    Ui::vpnChangelog *ui;

    void updateChangelogReadFlag();
    void buildChangelog();

private slots:
    void on_btnOK_clicked();

protected:
    bool eventFilter(QObject *object, QEvent *event);
};

#endif // VPNCHANGELOG_H
