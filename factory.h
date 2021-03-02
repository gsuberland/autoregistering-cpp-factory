#pragma once

//#define FACTORY_USE_UNIQUEPTR
//#define FACTORY_NO_MAKE_UNIQUE_SHIM

#ifdef FACTORY_USE_UNIQUEPTR
#include <memory>
#ifndef FACTORY_NO_MAKE_UNIQUE_SHIM
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif
#define FACTORY_T_PTR std::unique_ptr<T>
#else
#define FACTORY_T_PTR T*
#endif

struct cmp_cstr
{
   bool operator()(char const *a, char const *b) const
   {
      return strcmp(a, b) < 0;
   }
};

template<typename T, typename ...TArgs>
class Factory
{
  public:
    Factory() = delete;

    static bool Register(const char* name, FACTORY_T_PTR(*funcCreate)(TArgs...));

    static FACTORY_T_PTR Create(const char* name, TArgs... args);

    static bool IsRegistered(const char* name);

  private:
    static std::map<const char*, FACTORY_T_PTR(*)(TArgs...), cmp_cstr>* GetMap()
    {
      static std::map<const char*, FACTORY_T_PTR(*)(TArgs...), cmp_cstr> registeredTypesInternal;
      return &registeredTypesInternal;
    }

  public:
    static size_t GetCount()
    {
      return GetMap()->size();
    }

    static const char* GetNameByIndex(size_t index)
    {
#if ARDUINO
      assert(index < GetMap()->size());
#else
      if (index >= GetMap()->size())
      {
        return nullptr;
      }
#endif
      auto &nth_element = *std::next(GetMap()->begin(), index);
      return nth_element.first;
    }
};

template<typename T, typename ...TArgs>
bool Factory<T, TArgs...>::Register(const char* name, FACTORY_T_PTR(*funcCreate)(TArgs...))
{
  auto registeredTypes = GetMap();
  auto typeTuple = registeredTypes->find(name);
  if (typeTuple == registeredTypes->end())
  {
    (*registeredTypes)[name] = funcCreate;
    return true;
  }
  return false;
}

template<typename T, typename ...TArgs>
FACTORY_T_PTR Factory<T, TArgs...>::Create(const char* name, TArgs... args)
{
  auto registeredTypes = GetMap();
  auto typeTuple = registeredTypes->find(name);
  if (typeTuple != registeredTypes->end())
  {
    FACTORY_T_PTR(*funcCreate)(TArgs...) = typeTuple->second;
    return funcCreate(args...);
  }
  return nullptr;
}

template<typename T, typename ...TArgs>
bool Factory<T, TArgs...>::IsRegistered(const char* name)
{
  auto registeredTypes = GetMap();
  auto typeTuple = registeredTypes->find(name);
  if (typeTuple != registeredTypes->end())
      return true;
  return false;
}

template<typename TParent, typename TClass, typename ...TArgs>
class RegisteredInFactory
{
  protected:
    static bool _FACTORY_INIT;
};

template<typename TParent, typename TClass, typename ...TArgs>
bool RegisteredInFactory<TParent, TClass, TArgs...>::_FACTORY_INIT = Factory<TParent, TArgs...>::Register(TClass::GetFactoryKey(), TClass::CreateInstance);

#define FACTORY_INIT (void)_FACTORY_INIT;
