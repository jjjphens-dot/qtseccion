#pragma once
#include "core/requests.h"
#include <QDialog>

class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;

namespace takeout {
class RegisterDialog final : public QDialog {
  Q_OBJECT
public:
  enum class Mode { BootstrapAdmin, RegisterAccount };
  explicit RegisterDialog(Mode mode, QWidget *parent = nullptr);
  Mode mode() const { return m_mode; }
  AdminBootstrapRequest adminRequest() const;
  RegisterRequest accountRequest() const;
  MerchantRegistration merchantRequest() const;
  void clearPassword();
  void setError(const QString &message);
  void setBusy(bool busy);
  bool isBusy() const { return m_busy; }
  void accept() override;
  void complete();

signals:
  void submitted();

private:
  void updateFields();
  Mode m_mode;
  QFormLayout *m_form;
  QComboBox *m_role;
  QLineEdit *m_loginName;
  QLineEdit *m_password;
  QLineEdit *m_confirmPassword;
  QLineEdit *m_displayName;
  QLabel *m_addressLabel;
  QLineEdit *m_address;
  QLabel *m_shopNameLabel;
  QLineEdit *m_shopName;
  QLabel *m_descriptionLabel;
  QTextEdit *m_description;
  QLabel *m_error;
  QPushButton *m_submit;
  QPushButton *m_cancel;
  bool m_busy = false;
};
} // namespace takeout
