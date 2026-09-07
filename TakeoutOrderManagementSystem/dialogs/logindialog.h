#pragma once
#include "core/enums.h"
#include <QDialog>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace takeout {
class LoginDialog final : public QDialog {
  Q_OBJECT
public:
  enum { RegisterRequested = 2 };
  explicit LoginDialog(QWidget *parent = nullptr);
  QString loginName() const;
  QString password() const;
  Role role() const;
  void setError(const QString &message);

private:
  QComboBox *m_role;
  QLineEdit *m_loginName;
  QLineEdit *m_password;
  QLabel *m_error;
  QPushButton *m_register;
};
} // namespace takeout
