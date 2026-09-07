#include "registerdialog.h"
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace takeout {
RegisterDialog::RegisterDialog(Mode mode, QWidget *parent)
    : QDialog(parent), m_mode(mode), m_form(new QFormLayout),
      m_role(new QComboBox(this)), m_loginName(new QLineEdit(this)),
      m_password(new QLineEdit(this)), m_confirmPassword(new QLineEdit(this)),
      m_displayName(new QLineEdit(this)), m_addressLabel(new QLabel(this)),
      m_address(new QLineEdit(this)), m_shopNameLabel(new QLabel(this)),
      m_shopName(new QLineEdit(this)), m_descriptionLabel(new QLabel(this)),
      m_description(new QTextEdit(this)), m_error(new QLabel(this)) {
  setWindowTitle(mode == Mode::BootstrapAdmin ? QStringLiteral("初始化管理员")
                                              : QStringLiteral("注册账号"));
  setModal(true);
  setMinimumWidth(460);
  m_role->setObjectName("registrationRole");
  m_loginName->setObjectName("registrationLoginName");
  m_password->setObjectName("registrationPassword");
  m_confirmPassword->setObjectName("registrationPasswordConfirm");
  m_displayName->setObjectName("registrationDisplayName");
  m_address->setObjectName("registrationAddress");
  m_shopName->setObjectName("registrationShopName");
  m_description->setObjectName("registrationShopDescription");
  m_error->setObjectName("formError");
  m_password->setEchoMode(QLineEdit::Password);
  m_confirmPassword->setEchoMode(QLineEdit::Password);
  m_description->setMaximumHeight(90);
  m_error->setWordWrap(true);
  m_error->setVisible(false);
  if (mode == Mode::BootstrapAdmin) {
    m_role->addItem(roleLabel(Role::Admin), int(Role::Admin));
    m_role->setEnabled(false);
  } else {
    for (Role role : {Role::Customer, Role::Merchant, Role::Rider})
      m_role->addItem(roleLabel(role), int(role));
  }
  m_form->addRow(QStringLiteral("角色"), m_role);
  m_form->addRow(QStringLiteral("账号"), m_loginName);
  m_form->addRow(QStringLiteral("密码"), m_password);
  m_form->addRow(QStringLiteral("确认密码"), m_confirmPassword);
  m_form->addRow(QStringLiteral("姓名/显示名"), m_displayName);
  m_addressLabel->setText(QStringLiteral("地址"));
  m_form->addRow(m_addressLabel, m_address);
  m_shopNameLabel->setText(QStringLiteral("店铺名称"));
  m_form->addRow(m_shopNameLabel, m_shopName);
  m_descriptionLabel->setText(QStringLiteral("店铺简介"));
  m_form->addRow(m_descriptionLabel, m_description);
  auto *buttons = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  buttons->button(QDialogButtonBox::Save)
      ->setText(mode == Mode::BootstrapAdmin ? QStringLiteral("创建管理员")
                                             : QStringLiteral("注册"));
  auto *layout = new QVBoxLayout(this);
  layout->addLayout(m_form);
  layout->addWidget(m_error);
  layout->addWidget(buttons);
  connect(buttons, &QDialogButtonBox::accepted, this, &RegisterDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(m_role, &QComboBox::currentIndexChanged, this,
          &RegisterDialog::updateFields);
  updateFields();
}

void RegisterDialog::updateFields() {
  const Role selected = Role(m_role->currentData().toInt());
  const bool customer = selected == Role::Customer;
  const bool merchant = selected == Role::Merchant;
  m_addressLabel->setVisible(customer || merchant);
  m_address->setVisible(customer || merchant);
  m_addressLabel->setText(merchant ? QStringLiteral("店铺地址")
                                   : QStringLiteral("配送地址"));
  m_shopNameLabel->setVisible(merchant);
  m_shopName->setVisible(merchant);
  m_descriptionLabel->setVisible(merchant);
  m_description->setVisible(merchant);
}

void RegisterDialog::accept() {
  if (m_password->text() != m_confirmPassword->text()) {
    setError(QStringLiteral("两次输入的密码不一致"));
    return;
  }
  QDialog::accept();
}

AdminBootstrapRequest RegisterDialog::adminRequest() const {
  return {m_loginName->text(), m_password->text(), m_displayName->text()};
}
RegisterRequest RegisterDialog::accountRequest() const {
  return {m_loginName->text(), m_password->text(), m_displayName->text(),
          m_address->text(), Role(m_role->currentData().toInt())};
}
MerchantRegistration RegisterDialog::merchantRequest() const {
  return {m_loginName->text(),          m_password->text(),
          m_displayName->text(),        m_shopName->text(),
          m_description->toPlainText(), m_address->text()};
}
void RegisterDialog::setError(const QString &message) {
  m_error->setText(message);
  m_error->setVisible(true);
  m_password->clear();
  m_confirmPassword->clear();
}
} // namespace takeout
