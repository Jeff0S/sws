/******************************************************************************
/ configvar.h
/
/ Copyright (c) 2019 Christian Fillion
/ https://cfillion.ca
/
/ Permission is hereby granted, free of charge, to any person obtaining a copy
/ of this software and associated documentation files (the "Software"), to deal
/ in the Software without restriction, including without limitation the rights to
/ use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
/ of the Software, and to permit persons to whom the Software is furnished to
/ do so, subject to the following conditions:
/
/ The above copyright notice and this permission notice shall be included in all
/ copies or substantial portions of the Software.
/
/ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
/ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
/ OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
/ NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
/ HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
/ WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
/ FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
/ OTHER DEALINGS IN THE SOFTWARE.
/
******************************************************************************/

#pragma once

template<typename T>
class ConfigVar {
public:
  ConfigVar(const char *name, ReaProject *project = nullptr)
    : m_name{name}, m_addr{}
  {
    int size = 0;
    void *addr = nullptr;

    if(const int offset = projectconfig_var_getoffs(name, &size))
      addr = projectconfig_var_addr(project, offset);
    else
      addr = get_config_var(name, &size);

    if(size == sizeof(T))
      m_addr = static_cast<T *>(addr);
  }

  explicit operator bool() const { return m_addr != nullptr; }

  T &operator*() { return *m_addr; }
  const T &operator*() const { return *m_addr; }
  T *get() { return m_addr; }
  const T *get() const { return m_addr; }

  T value_or(const T fallback) const
  {
    return m_addr ? *m_addr : fallback;
  }

  bool try_set(const T newValue)
  {
    if(!m_addr)
      return false;

    *m_addr = newValue;
    return true;
  }

  void save();

private:
  const char *m_name;
  T *m_addr;
};

template<typename T>
class ConfigVarOverride {
public:
  ConfigVarOverride(ConfigVar<T> var, const T tempValue)
    : m_var{var}
  {
    if(m_var) {
      m_initialValue = *m_var;
      *m_var = tempValue;
    }
  }

  void rollback()
  {
    m_var.try_set(m_initialValue);
  }

  ~ConfigVarOverride()
  {
    rollback();
  }

private:
  ConfigVar<T> m_var;
  T m_initialValue;
};
