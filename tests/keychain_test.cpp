#include "session.h"

#include <QTest>

#include <qt6keychain/keychain.h>

using namespace legato;

// A failed keychain job either means there is no keychain, which disables "Keep me signed in", or
// that the keychain refused one entry, which must leave it enabled.
class KeychainTest : public QObject
{
    Q_OBJECT

private slots:
    void aRefusedEntryKeepsTheKeychain()
    {
        // oo7-daemon answers a write with OtherError ("Password too short: 1") while running.
        QVERIFY(!keychainMissing(QKeychain::OtherError, true));
        QVERIFY(!keychainMissing(QKeychain::AccessDenied, true));
        QVERIFY(!keychainMissing(QKeychain::AccessDeniedByUser, false));
        QVERIFY(!keychainMissing(QKeychain::EntryNotFound, true));
    }

    void noServiceMeansNoKeychain()
    {
        // No Secret Service on the bus also arrives as OtherError.
        QVERIFY(keychainMissing(QKeychain::OtherError, false));
        QVERIFY(keychainMissing(QKeychain::NoBackendAvailable, true));
        QVERIFY(keychainMissing(QKeychain::NotImplemented, true));
    }
};

QTEST_GUILESS_MAIN(KeychainTest)
#include "keychain_test.moc"
