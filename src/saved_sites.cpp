#include "mainwindow.h"

#include <QComboBox>
#include <QDir>
#include <QLineEdit>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>

void
MainWindow::loadSavedSites()
{
  QSettings settings(QDir::homePath() + "/.WitechFTP/settings.ini", QSettings::IniFormat);
  int size = settings.beginReadArray("SavedSites");
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QString host = settings.value("host").toString();
    QString username = settings.value("username").toString();
    QString password = settings.value("password").toString();

    QVariantMap siteData;
    siteData["host"] = host;
    siteData["username"] = username;
    siteData["password"] = password;

    savedSitesComboBox->addItem(QString("%1 (%2)").arg(host, username), siteData);
  }
  settings.endArray();
}

void
MainWindow::saveCurrentSite()
{
  QString currentHost = hostLineEdit->text().trimmed();
  QString currentUsername = usernameLineEdit->text().trimmed();
  QString currentPassword = passwordLineEdit->text();

  if (currentHost.isEmpty()) return;

  QSettings settings(QDir::homePath() + "/.WitechFTP/settings.ini", QSettings::IniFormat);

  // Read existing
  QList<QVariantMap> sites;
  int size = settings.beginReadArray("SavedSites");
  for (int i = 0; i < size; ++i) {
    settings.setArrayIndex(i);
    QVariantMap siteData;
    siteData["host"] = settings.value("host").toString();
    siteData["username"] = settings.value("username").toString();
    siteData["password"] = settings.value("password").toString();
    sites.append(siteData);
  }
  settings.endArray();

  // Check if it already exists
  bool exists = false;
  for (int i = 0; i < sites.size(); ++i) {
    if (sites[i]["host"].toString() == currentHost && sites[i]["username"].toString() == currentUsername) {
      // Update password if changed
      sites[i]["password"] = currentPassword;
      exists = true;

      // Update combo box
      for (int j = 1; j < savedSitesComboBox->count(); ++j) {
        QVariantMap data = savedSitesComboBox->itemData(j).toMap();
        if (data["host"].toString() == currentHost && data["username"].toString() == currentUsername) {
          savedSitesComboBox->setItemData(j, sites[i]);
          break;
        }
      }
      break;
    }
  }

  if (!exists) {
    QVariantMap newSite;
    newSite["host"] = currentHost;
    newSite["username"] = currentUsername;
    newSite["password"] = currentPassword;
    sites.append(newSite);

    // Add to UI combobox as well
    savedSitesComboBox->addItem(QString("%1 (%2)").arg(currentHost, currentUsername), newSite);
  }

  // Save back
  settings.beginWriteArray("SavedSites");
  for (int i = 0; i < sites.size(); ++i) {
    settings.setArrayIndex(i);
    settings.setValue("host", sites[i]["host"]);
    settings.setValue("username", sites[i]["username"]);
    settings.setValue("password", sites[i]["password"]);
  }
  settings.endArray();
}

void
MainWindow::onSavedSiteSelected(int index)
{
  if (index == 0) return; // "Sparade sajter..."

  QVariant data = savedSitesComboBox->itemData(index);
  QVariantMap siteData = data.toMap();

  if (!siteData.isEmpty()) {
    hostLineEdit->setText(siteData["host"].toString());
    usernameLineEdit->setText(siteData["username"].toString());
    passwordLineEdit->setText(siteData["password"].toString());

    if (m_ftpCommunicator->isConnected()) {
      // Koppla från automatiskt
      connectOrDisconnect();

      // Vänta en halv sekund så anslutningen hinner stängas, anslut sedan igen
      QTimer::singleShot(500, this, &MainWindow::connectOrDisconnect);
    } else {
      // Auto-connect!
      connectOrDisconnect();
    }
  }

  // Återställ rullgardinsmenyn så den fungerar som en åtgärdsmeny
  savedSitesComboBox->setCurrentIndex(0);
}
