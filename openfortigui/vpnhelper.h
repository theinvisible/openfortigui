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

#ifndef VPNHELPER_H
#define VPNHELPER_H

#include <QString>

struct vpnHelperResult
{
    bool status;
    QString msg;
    QString data;
};

class vpnHelper
{
public:
    vpnHelper();

    static QString formatByteUnits(qint64 num);

    static vpnHelperResult checkSystemPasswordStoreAvailable();
    static vpnHelperResult systemPasswordStoreWrite(const QString &key, const QString &data);
    static vpnHelperResult systemPasswordStoreRead(const QString &key);
    static vpnHelperResult systemPasswordStoreDelete(const QString &key);

    static int aes128_encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key, unsigned char *iv, unsigned char *ciphertext);
    static int aes128_decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key,unsigned char *iv, unsigned char *plaintext);
    static QString Qaes128_encrypt(const QString &plain, const QString &key, const QString &iv);
    static QString Qaes128_decrypt(const QString &cipher, const QString &key, const QString &iv);

    static void ssl_handleErrors(void);
};

#endif // VPNHELPER_H
