#pragma once

/*
 * Generic auto-registering factory implementation, designed for C++11.
 * Copyright (c) 2021 - Graham Sutherland. Released under MIT license.
 * https://github.com/gsuberland/autoregistering-cpp-factory
 * 
 */

// define this if you want to use unique_ptr<T> as the return type of Factory<>::Create()
//#define FACTORY_USE_UNIQUEPTR
// define this if you're using unique_ptr<T>, but don't want the code to include a C++11 shim version of make_unique<T>
//#define FACTORY_NO_MAKE_UNIQUE_SHIM
// define this if you're on Arduino and you don't want to have an assertion failure on trying to create a type that isn't in the factory
//#define FACTORY_NO_ARDUINO_ASSERT

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

// compare type for map<> so we can use const char* as a key
struct cmp_cstr
{
   bool operator()(char const *a, char const *b) const
   {
      return strcmp(a, b) < 0;
   }
};

// main factory type. T is the base type for the factory, TArgs are the types of the arguments passed to the constructor
template<typename T, typename ...TArgs>
class Factory
{
  public:
    Factory() = delete;

    // registration function. takes a unique name/key for the type, and a pointer to a function that creates an instance of the type.
    // FACTORY_T_PTR is usually T*, but it may be unique_ptr<T>
    static bool Register(const char* name, FACTORY_T_PTR(*funcCreate)(TArgs...));

    // creates an instance of a type, by its name, with the provided constructor arguments.
    static FACTORY_T_PTR Create(const char* name, TArgs... args);

    // returns true if a type is registered with a factory with the given name.
    static bool IsRegistered(const char* name);

  private:
    // internal function that is used to get at the map, initialising it on first access.
    // this is how we avoid undefined behaviour with static member initialisation order >:)
    static std::map<const char*, FACTORY_T_PTR(*)(TArgs...), cmp_cstr>* GetMap()
    {
      static std::map<const char*, FACTORY_T_PTR(*)(TArgs...), cmp_cstr> registeredTypesInternal;
      return &registeredTypesInternal;
    }

  public:
    // get the number of types registered in the factory
    static size_t GetCount()
    {
      return GetMap()->size();
    }

    // get the name of the type at the given index
    // WARNING: this is O(n) on the index, so if you call this for every index in the factory this is O(n log n). this only matters in critical paths.
    // if you want to access by index instead of name, I recommend keeping your own map of the indices to names.
    static const char* GetNameByIndex(size_t index)
    {
#if ARDUINO && !FACTORY_NO_ARDUINO_ASSERT
      // assertion fail on Arduino, rather than returning nullptr.
      // this is personal preference. you can turn it off with FACTORY_NO_ARDUINO_ASSERT.
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

// register a type with the factory
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

// create a type by name
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

// return true if the given type is registered
template<typename T, typename ...TArgs>
bool Factory<T, TArgs...>::IsRegistered(const char* name)
{
  auto registeredTypes = GetMap();
  auto typeTuple = registeredTypes->find(name);
  if (typeTuple != registeredTypes->end())
      return true;
  return false;
}

// helper type that allows us to do much of the registration work simply by having classes inherit from this thing
template<typename TParent, typename TClass, typename ...TArgs>
class RegisteredInFactory
{
  protected:
    // this is the static field whose initialisation triggers the registration
    static bool _FACTORY_INIT;
};

// registration initialiser
template<typename TParent, typename TClass, typename ...TArgs>
bool RegisteredInFactory<TParent, TClass, TArgs...>::_FACTORY_INIT = Factory<TParent, TArgs...>::Register(TClass::GetFactoryKey(), TClass::CreateInstance);

// reference this in your class constructors to prevent the compiler from yeeting the initialiser and template during optimisation
#define FACTORY_INIT (void)_FACTORY_INIT;
