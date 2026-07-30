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
#include <QProcess>
#include <QDateTime>
#include <QLocalSocket>

#include "config.h"
#include "vpnapi.h"
#include <qt6keychain/keychain.h>

#include <openssl/conf.h>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <QFile>
#include <QDir>
#include <pwd.h>

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

/*
 * Time since a unix timestamp as H:mm:ss, or an empty string when the tunnel is
 * not up (vpn_start == 0).
 */
QString vpnHelper::formatDuration(qint64 since_epoch_secs)
{
    if(since_epoch_secs <= 0)
        return QString();

    const qint64 secs = QDateTime::currentSecsSinceEpoch() - since_epoch_secs;
    if(secs < 0)
        return QString();

    return QString("%1:%2:%3")
            .arg(secs / 3600)
            .arg((secs % 3600) / 60, 2, 10, QLatin1Char('0'))
            .arg(secs % 60, 2, 10, QLatin1Char('0'));
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

/*
 * Returns the plaintext length, or -1 on error.
 *
 * Every step is checked and bailed out of. It used to only print the OpenSSL
 * error and carry on, so a failed decryption produced a length built from an
 * uninitialised "len" -- and the caller got garbage that looked like a
 * password. That is what turned a wrong AES key into "Could not authenticate
 * to gateway (No cookie given)" with no hint of the real cause (issues #160,
 * #201).
 */
int vpnHelper::aes128_decrypt(unsigned char *ciphertext, int ciphertext_len, unsigned char *key, unsigned char *iv, unsigned char *plaintext)
{
    EVP_CIPHER_CTX *ctx;

    int len = 0;
    int plaintext_len = 0;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new()))
    {
        ssl_handleErrors();
        return -1;
    }

    /* Initialise the decryption operation. IMPORTANT - ensure you use a key
    * and IV size appropriate for your cipher
    * In this example we are using 128 bit AES (i.e. a 128 bit key). The
    * IV size for *most* modes is the same as the block size. For AES this
    * is 128 bits */
    if(1 != EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), NULL, key, iv))
    {
        ssl_handleErrors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    /* Provide the message to be decrypted, and obtain the plaintext output.
    * EVP_DecryptUpdate can be called multiple times if necessary
    */
    if(1 != EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, ciphertext_len))
    {
        ssl_handleErrors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len = len;

    /* Finalise the decryption. Further plaintext bytes may be written at
    * this stage.
    */
    if(1 != EVP_DecryptFinal_ex(ctx, plaintext + len, &len))
    {
        // Wrong key/IV, or the value is not what we wrote -- padding check failed.
        ssl_handleErrors();
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    plaintext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return plaintext_len;
}

bool vpnHelper::aesKeyUsable(const QString &key, const QString &iv)
{
    // AES-128-CBC: both have to be exactly 16 bytes. Anything else and OpenSSL
    // reads past the end of what we hand it.
    return key.toUtf8().length() == 16 && iv.toUtf8().length() == 16;
}

QString vpnHelper::Qaes128_encrypt(const QString &plain, const QString &key, const QString &iv)
{
    if(plain.isEmpty())
        return "";

    if(!aesKeyUsable(key, iv))
    {
        qWarning() << "vpnHelper::Qaes128_encrypt:: AES key/IV are not 16 bytes, refusing to encrypt";
        return "";
    }

    QByteArray plainBytes = plain.toUtf8();
    QByteArray keyBytes = key.toUtf8();
    QByteArray ivBytes = iv.toUtf8();
    QByteArray tmp;
    // CBC pads up to a full block, so the output can be longer than the input.
    // "length * 10" was too small for anything shorter than two characters.
    tmp.resize(plainBytes.length() + EVP_MAX_BLOCK_LENGTH);
    int ciphertext_len;

    ciphertext_len = vpnHelper::aes128_encrypt(reinterpret_cast<unsigned char *>(plainBytes.data()), plainBytes.length(), reinterpret_cast<unsigned char *>(keyBytes.data()), reinterpret_cast<unsigned char *>(ivBytes.data()), reinterpret_cast<unsigned char *>(tmp.data()));
    if(ciphertext_len <= 0)
    {
        qWarning() << "vpnHelper::Qaes128_encrypt:: encryption failed";
        return "";
    }

    tmp.resize(ciphertext_len);
    return QString::fromUtf8(tmp.toBase64());
}

QString vpnHelper::Qaes128_decrypt(const QString &cipher, const QString &key, const QString &iv)
{
    if(cipher.isEmpty())
        return "";

    if(!aesKeyUsable(key, iv))
    {
        qWarning() << "vpnHelper::Qaes128_decrypt:: AES key/IV are not 16 bytes -- check main/aeskey and main/aesiv in your configuration";
        return "";
    }

    QByteArray keyBytes = key.toUtf8();
    QByteArray ivBytes = iv.toUtf8();
    QByteArray tmp, ci;
    ci = QByteArray::fromBase64(cipher.toUtf8());
    tmp.resize(ci.length() + EVP_MAX_BLOCK_LENGTH);
    int decryptedtext_len;

    decryptedtext_len = vpnHelper::aes128_decrypt(reinterpret_cast<unsigned char *>(ci.data()), ci.length(), reinterpret_cast<unsigned char *>(keyBytes.data()), reinterpret_cast<unsigned char *>(ivBytes.data()), reinterpret_cast<unsigned char *>(tmp.data()));
    if(decryptedtext_len < 0)
    {
        // Do not hand back garbage: an empty result makes the caller ask for a
        // password instead of trying to authenticate with random bytes.
        qWarning() << "vpnHelper::Qaes128_decrypt:: could not decrypt the stored value -- wrong AES key/IV?";
        return "";
    }

    tmp.resize(decryptedtext_len);
    return QString::fromUtf8(tmp);
}

void vpnHelper::ssl_handleErrors()
{
    ERR_print_errors_fp(stderr);
}

QString vpnHelper::runCommandwithOutput(const QString &cmd)
{
    QProcess proc;
    proc.startCommand(cmd, QIODevice::ReadOnly);
    proc.waitForStarted();
    proc.waitForFinished();

    return proc.readLine();
}

int vpnHelper::runCommandwithReturnCode(const QString &cmd)
{
    QProcess proc;
    proc.startCommand(cmd, QIODevice::ReadOnly);
    proc.waitForStarted();
    proc.waitForFinished();

    return proc.exitCode();
}

QString vpnHelper::linHomeExpansion(const QString &path) {
    if (!path.startsWith(QLatin1Char('~')))
        return path;
    int separatorPosition = path.indexOf(QDir::separator());
    if (separatorPosition < 0)
        separatorPosition = path.size();
    if (separatorPosition == 1) {
        return QDir::homePath() + path.mid(1);
    } else {
#if defined(Q_OS_VXWORKS) || defined(Q_OS_INTEGRITY)
        const QString homePath = QDir::homePath();
#else
        const QByteArray userName = path.mid(1, separatorPosition - 1).toLocal8Bit();
# if defined(_POSIX_THREAD_SAFE_FUNCTIONS) && !defined(Q_OS_OPENBSD) && !defined(Q_OS_WASM)
        passwd pw;
        passwd *tmpPw;
        char buf[200];
        const int bufSize = sizeof(buf);
        int err = 0;
#  if defined(Q_OS_SOLARIS) && (_POSIX_C_SOURCE - 0 < 199506L)
        tmpPw = getpwnam_r(userName.constData(), &pw, buf, bufSize);
#  else
        err = getpwnam_r(userName.constData(), &pw, buf, bufSize, &tmpPw);
#  endif
        if (err || !tmpPw)
            return path;
        const QString homePath = QString::fromLocal8Bit(pw.pw_dir);
# else
        passwd *pw = getpwnam(userName.constData());
        if (!pw)
            return path;
        const QString homePath = QString::fromLocal8Bit(pw->pw_dir);
# endif
#endif
        return homePath + path.mid(separatorPosition);
    }
}

/*
 * Is a GUI instance listening on the api socket?
 *
 * A ping over the socket, not a process listing: what callers want to know is
 * whether there is an instance they can talk to. Used by the KRunner plugin
 * before it starts one. (MainWindow's own isRunningAlready() in main.cpp still
 * counts processes with "ps", which also counts running VPN children -- worth
 * replacing with this one day.)
 */
bool vpnHelper::isOpenFortiGUIRunning()
{
    QLocalSocket apiServerTest;
    apiServerTest.connectToServer(vpnApi::socketPath());
    if(!apiServerTest.waitForConnected(200))
        return false;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_6_0);
    vpnApi apiData;
    apiData.objName = "ping";
    apiData.action = vpnApi::ACTION_PING;
    out << apiData;

    apiServerTest.write(block);
    apiServerTest.flush();
    apiServerTest.close();

    return true;
}
