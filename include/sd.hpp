#include "FS.h"

class sd_card {
  public:
    explicit sd_card();
    bool        open_sd(const char* name);
    void        write_sd(const char* msg);
    bool        close_sd();
    [[nodiscard]] bool        init_sd() const;
    bool        is_open  = false;
    bool        is_write = false;
    const char* name;

  private:
    File      m_log_file_;
    const int m_chip_select_ = 5;
    int64_t      m_last_flush_  = 0;
};
