#pragma once

#include "core/result.h"

namespace takeout {

// The seam keeps QSaveFile's atomic write/commit behavior in production while
// allowing deterministic write and commit failure tests.
class AtomicFileWriter {
public:
  virtual ~AtomicFileWriter() = default;
  virtual Result<void> write(const QString &path,
                             const QByteArray &bytes) const = 0;
};

class QSaveFileWriter final : public AtomicFileWriter {
public:
  Result<void> write(const QString &path,
                     const QByteArray &bytes) const override;
};

} // namespace takeout
