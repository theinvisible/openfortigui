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

#include "vpnhelper.h"

#include <QEventLoop>

#include "config.h"
#include <qt5keychain/keychain.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>

vpnHelper::vpnHelper()
{

}

QString vpnHelper::formatByteUnits(qint64 num)
{
    if(num >= 1024*1024*1024)
        return QString("%1G").arg(QString::number((double)num / (1024*1024*1024), 'f', 2));
    else if(num >= 1024*1024)
        return QString("%1M").arg(QString::number((double)num / (1024*1024), 'f', 2));
    else if(num >= 1024)
        return QString("%1K").arg(QString::number((double)num / 1024, 'f', 2));
    else
        return QString("%1B").arg(num);
}

vpnHelperResult vpnHelper::checkSystemPasswordStoreAvailable()
{
    vpnHelperResult result;
    result.status = false;

    QKeychain::WritePasswordJob job ((QLatin1String(openfortigui_config::password_manager_namespace)));
    job.setAutoDelete(false);
    job.setKey("testkeystore");
    job.setBinaryData("test");
    QEventLoop loop;
    job.connect(&job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
    job.start();
    loop.exec();
    if(job.error())
    {
        result.status = false;
        result.msg = job.errorString();
        return result;
    }

    QKeychain::DeletePasswordJob job2 ((QLatin1String(openfortigui_config::password_manager_namespace)));
    job2.setAutoDelete(false);
    job2.setKey("testkeystore");
    QEventLoop loop2;
    job2.connect(&job2, SIGNAL(finished(QKeychain::Job*)), &loop2, SLOT(quit()));
    job2.start();
    loop2.exec();
    if(job2.error())
    {
        result.status = false;
        result.msg = job2.errorString();
        return result;
    }

    result.status = true;
    return result;
}

vpnHelperResult vpnHelper::systemPasswordStoreWrite(const QString &key, const QString &data)
{
    vpnHelperResult result;
    result.status = false;

    QKeychain::WritePasswordJob job ((QLatin1String(openfortigui_config::password_manager_namespace)));
    job.setAutoDelete(false);
    job.setKey(key);
    job.setBinaryData(data.toUtf8());
    QEventLoop loop;
    job.connect(&job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
    job.start();
    loop.exec();
    if(job.error())
    {
        result.status = false;
        result.msg = job.errorString();
        return result;
    }

    result.status = true;
    return result;
}

vpnHelperResult vpnHelper::systemPasswordStoreRead(const QString &key)
{
    vpnHelperResult result;
    result.status = false;
    result.data = "";

    QKeychain::ReadPasswordJob job ((QLatin1String(openfortigui_config::password_manager_namespace)));
    job.setAutoDelete(false);
    job.setKey(key);
    QEventLoop loop;
    job.connect(&job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
    job.start();
    loop.exec();

    const QString pw = job.textData();
    if(job.error())
    {
        result.status = false;
        result.msg = job.errorString();
        return result;
    }

    result.status = true;
    result.data = pw;
    return result;
}

vpnHelperResult vpnHelper::systemPasswordStoreDelete(const QString &key)
{
    vpnHelperResult result;
    result.status = false;

    QKeychain::DeletePasswordJob job ((QLatin1String(openfortigui_config::password_manager_namespace)));
    job.setAutoDelete(false);
    job.setKey(key);
    QEventLoop loop;
    job.connect(&job, SIGNAL(finished(QKeychain::Job*)), &loop, SLOT(quit()));
    job.start();
    loop.exec();
    if(job.error())
    {
        result.status = false;
        result.msg = job.errorString();
        return result;
    }

    result.status = true;
    return result;
}

int vpnHelper::aes128_encrypt(unsigned char *plaintext, int plaintext_len, unsigned char *key, unsigned char *iv, unsigned char *ciphertext)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int ciphertext_len;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
        ssl_handleErrors();

    /* Initialise the encryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 128 bit AES (i.e. a 128 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits */
    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv))
        ssl_handleErrors();

    /* Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
        ssl_handleErrors();
    ciphertext_len = len;

    /* Finalise the encryption. Further ciphertext bytes may be written at
     * this stage.
     */
    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len))
        ssl_handleErrors();
    ciphertext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext_len;
}

int vpnHelper::aes128_decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key, unsigned char *iv, unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int plaintext_len;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
        ssl_handleErrors();

    /* Initialise the decryption operation. IMPORTANT - ensure you use a key
    * and IV size appropriate for your cipher
    * In this example we are using 128 bit AES (i.e. a 128 bit key). The
    * IV size for *most* modes is the same as the block size. For AES this
    * is 128 bits */
    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv))
        ssl_handleErrors();

    /* Provide the message to be decrypted, and obtain the plaintext output.
    * EVP_DecryptUpdate can be called multiple times if necessary
    */
    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
        ssl_handleErrors();
    plaintext_len = len;

    /* Finalise the decryption. Further plaintext bytes may be written at
    * this stage.
    */
    if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
        ssl_handleErrors();
    plaintext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;
}

QString vpnHelper::Qaes128_encrypt(const QString &plain, const QString &key, const QString &iv)
{
    if(plain.isEmpty())
        return "";

    QByteArray tmp;
    tmp.resize(plain.length() * 10);
    int ciphertext_len;

    ciphertext_len = vpnHelper::aes128_encrypt((unsigned char *) plain.toStdString().c_str(), plain.toStdString().length(), (unsigned char *) key.toStdString().c_str(), (unsigned char *) iv.toStdString().c_str(), (unsigned char *) tmp.data());
    tmp.resize(ciphertext_len);
    return QString::fromUtf8(tmp.toBase64());
}

QString vpnHelper::Qaes128_decrypt(const QString &cipher, const QString &key, const QString &iv)
{
    if(cipher.isEmpty())
        return "";

    QByteArray tmp, ci;
    tmp.resize(cipher.length() * 10);
    int decryptedtext_len;
    ci = QByteArray::fromBase64(cipher.toUtf8());

    decryptedtext_len = vpnHelper::aes128_decrypt((unsigned char *) ci.data(), ci.length(), (unsigned char *) key.toStdString().c_str(), (unsigned char *) iv.toStdString().c_str(), (unsigned char *) tmp.data());
    tmp.resize(decryptedtext_len);
    return QString::fromUtf8(tmp);
}

void vpnHelper::ssl_handleErrors()
{
    ERR_print_errors_fp(stderr);
}

