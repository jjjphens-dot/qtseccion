#pragma once

#include "core/result.h"
#include <QWidget>

class QLabel;
class QPushButton;

namespace takeout {
class AdminService;

class AdminDataManagementWidget final : public QWidget {
  Q_OBJECT
public:
  explicit AdminDataManagementWidget(AdminService &admin,
                                     QWidget *parent = nullptr);

private:
  void exportData();
  void importData();
  void restoreBackup();
  void setStatus(const Result<void> &result, const QString &successMessage);

  AdminService &m_admin;
  QLabel *m_status = nullptr;
  QPushButton *m_exportButton = nullptr;
  QPushButton *m_importButton = nullptr;
  QPushButton *m_restoreButton = nullptr;
};
} // namespace takeout
