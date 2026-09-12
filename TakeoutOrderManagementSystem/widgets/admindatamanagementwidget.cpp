#include "admindatamanagementwidget.h"

#include "services/adminservice.h"
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace takeout {

AdminDataManagementWidget::AdminDataManagementWidget(AdminService &admin,
                                                     QWidget *parent)
    : QWidget(parent), m_admin(admin) {
  setObjectName(QStringLiteral("adminDataManagement"));
  auto *layout = new QVBoxLayout(this);
  auto *warning = new QLabel(
      QStringLiteral("完整数据备份包含认证凭据摘要，请勿提交 Git、上传公共平台或随意共享。"),
      this);
  warning->setObjectName(QStringLiteral("adminBackupWarning"));
  warning->setWordWrap(true);
  layout->addWidget(warning);

  auto *controls = new QHBoxLayout;
  m_exportButton = new QPushButton(QStringLiteral("导出完整数据备份"), this);
  m_importButton = new QPushButton(QStringLiteral("导入完整数据备份"), this);
  m_restoreButton = new QPushButton(QStringLiteral("恢复上一份备份"), this);
  m_status = new QLabel(this);
  m_status->setObjectName(QStringLiteral("adminDataStatus"));
  m_status->setWordWrap(true);
  controls->addWidget(m_exportButton);
  controls->addWidget(m_importButton);
  controls->addWidget(m_restoreButton);
  controls->addWidget(m_status, 1);
  layout->addLayout(controls);

  connect(m_exportButton, &QPushButton::clicked, this,
          &AdminDataManagementWidget::exportData);
  connect(m_importButton, &QPushButton::clicked, this,
          &AdminDataManagementWidget::importData);
  connect(m_restoreButton, &QPushButton::clicked, this,
          &AdminDataManagementWidget::restoreBackup);
}

void AdminDataManagementWidget::setStatus(const Result<void> &result,
                                          const QString &successMessage) {
  m_status->setText(result.ok() ? successMessage : result.error().message);
}

void AdminDataManagementWidget::exportData() {
  const auto path = QFileDialog::getSaveFileName(
      this, QStringLiteral("导出完整数据备份"),
      QStringLiteral("takeout-full-backup.json"),
      QStringLiteral("JSON 备份文件 (*.json);;所有文件 (*.*)"));
  if (path.isEmpty())
    return;
  setStatus(m_admin.exportData(path),
            QStringLiteral("完整数据备份已原子导出：%1").arg(path));
}

void AdminDataManagementWidget::importData() {
  const auto path = QFileDialog::getOpenFileName(
      this, QStringLiteral("导入完整数据备份"), {},
      QStringLiteral("JSON 备份文件 (*.json);;所有文件 (*.*)"));
  if (path.isEmpty())
    return;
  if (QMessageBox::warning(
          this, QStringLiteral("确认导入"),
          QStringLiteral("导入会替换当前业务数据，并要求管理员使用导入后的凭据重新登录。继续吗？"),
          QMessageBox::Yes | QMessageBox::No,
          QMessageBox::No) != QMessageBox::Yes)
    return;
  setStatus(m_admin.importData(path), QStringLiteral("数据已导入，请重新登录"));
}

void AdminDataManagementWidget::restoreBackup() {
  if (QMessageBox::warning(
          this, QStringLiteral("确认恢复备份"),
          QStringLiteral("将用上一份已验证备份替换当前数据，并要求管理员重新登录。继续吗？"),
          QMessageBox::Yes | QMessageBox::No,
          QMessageBox::No) != QMessageBox::Yes)
    return;
  setStatus(m_admin.restoreBackup(),
            QStringLiteral("已恢复上一份备份，请重新登录"));
}

} // namespace takeout
