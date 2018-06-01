/*
 *  Copyright (C) 2018 Rene Hadler
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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
