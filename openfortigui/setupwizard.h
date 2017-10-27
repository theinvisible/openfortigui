#ifndef SETUPWIZARD_H
#define SETUPWIZARD_H

#include <QWidget>

namespace Ui {
class setupWizard;
}

class setupWizard : public QWidget
{
    Q_OBJECT

public:
    explicit setupWizard(QWidget *parent = 0);
    ~setupWizard();

private:
    Ui::setupWizard *ui;
};

#endif // SETUPWIZARD_H
