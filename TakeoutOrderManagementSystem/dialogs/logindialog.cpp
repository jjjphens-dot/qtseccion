#include "logindialog.h"
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace takeout {
LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent), m_role(new QComboBox(this)),
      m_loginName(new QLineEdit(this)), m_password(new QLineEdit(this)),
      m_error(new QLabel(this)),
      m_submit(new QPushButton(QStringLiteral("登录"), this)),
      m_register(new QPushButton(this)) {
  setWindowTitle(QStringLiteral("账号登录"));
  setModal(true);
  setMinimumWidth(420);
  m_role->setObjectName("loginRole");
  m_loginName->setObjectName("loginName");
  m_password->setObjectName("loginPassword");
  m_error->setObjectName("formError");
  m_register->setObjectName("openRegistration");
  for (Role role : {Role::Customer, Role::Merchant, Role::Rider, Role::Admin})
    m_role->addItem(roleLabel(role), int(role));
  m_loginName->setPlaceholderText(QStringLiteral("3–32 位字母、数字或下划线"));
  m_password->setEchoMode(QLineEdit::Password);
  m_password->setPlaceholderText(QStringLiteral("密码"));
  m_error->setWordWrap(true);
  m_error->setVisible(false);

  auto *form = new QFormLayout;
  form->addRow(QStringLiteral("角色"), m_role);
  form->addRow(QStringLiteral("账号"), m_loginName);
  form->addRow(QStringLiteral("密码"), m_password);
  m_submit->setObjectName("submitLogin");
  m_register->setText(QStringLiteral("注册新账号"));
  auto *cancel = new QPushButton(QStringLiteral("取消"), this);
  auto *buttons = new QHBoxLayout;
  buttons->addWidget(m_register);
  buttons->addStretch();
  buttons->addWidget(cancel);
  buttons->addWidget(m_submit);
  auto *layout = new QVBoxLayout(this);
  layout->addLayout(form);
  layout->addWidget(m_error);
  layout->addLayout(buttons);
  connect(m_submit, &QPushButton::clicked, this,
          [this] {
            if (!m_busy)
              emit submitted();
          });
  connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_register, &QPushButton::clicked, this,
          [this] { done(RegisterRequested); });
  connect(m_role, &QComboBox::currentIndexChanged, this,
          [this] { m_register->setEnabled(role() != Role::Admin); });
}

QString LoginDialog::loginName() const { return m_loginName->text(); }
QString LoginDialog::password() const { return m_password->text(); }
void LoginDialog::clearPassword() { m_password->clear(); }
Role LoginDialog::role() const { return Role(m_role->currentData().toInt()); }
void LoginDialog::setError(const QString &message) {
  m_error->setText(message);
  m_error->setVisible(true);
  m_password->clear();
  m_password->setFocus();
}

void LoginDialog::setBusy(bool busy) {
  m_busy = busy;
  m_role->setEnabled(!busy);
  m_loginName->setEnabled(!busy);
  m_password->setEnabled(!busy);
  m_register->setEnabled(!busy && role() != Role::Admin);
  m_submit->setEnabled(!busy);
}
} // namespace takeout
