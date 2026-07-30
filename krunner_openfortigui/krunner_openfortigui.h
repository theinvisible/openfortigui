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

#include <QProcess>
#include <KRunner/AbstractRunner>

/*
 * KRunner 6. Compared to the KF5 version: the namespace is KRunner instead of
 * Plasma, the constructor takes the plugin metadata instead of a QVariantList,
 * and setSpeed()/setPriority()/setHasRunOptions() are gone -- KRunner runs every
 * runner in its own thread and no longer has per-runner run options.
 */
class Krunner_openfortigui : public KRunner::AbstractRunner
{
    Q_OBJECT

public:
    Krunner_openfortigui(QObject *parent, const KPluginMetaData &metaData);

    enum dataRunnerType
    {
        DATA_TYPE_VPN = 0,
        DATA_TYPE_VPNGROUP
    };

    void match(KRunner::RunnerContext &context) override;
    void run(const KRunner::RunnerContext &context, const KRunner::QueryMatch &match) override;
};

#endif // KRUNNER_OPENFORTIGUI_H
