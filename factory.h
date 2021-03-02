#pragma once

/*
 * Generic auto-registering factory implementation, designed for C++11.
 * Copyright (c) 2021 - Graham Sutherland. Released under MIT license.
 * https://github.com/gsuberland/autoregistering-cpp-factory
 * 
 */

// this code requires <map> and <cstring> to be included. uncomment them here if you're not including them elsewhere (e.g. using a PCH)
/*
#include <map>
#include <cstring>
*/

// define this if you want to use unique_ptr<T> as the return type of Factory<>::Create()
//#define FACTORY_USE_UNIQUEPTR
// define this if you're using unique_ptr<T>, but don't want the code to include a C++11 shim version of make_unique<T>
//#define FACTORY_NO_MAKE_UNIQUE_SHIM
// define this if you're on Arduino and you don't want to have an assertion failure on trying to create a type that isn't in the factory
//#define FACTORY_NO_ARDUINO_ASSERT

// define FACTORY_TYPE_NAME in your code if you want to change Factory<...> to some other name, in case of a name conflict.
#ifndef FACTORY_TYPE_NAME
  #define FACTORY_TYPE_NAME Factory
#endif

// define FACTORY_KEY_TYPE in your code if you want to change the type of the key from const char* to something else.
#ifndef FACTORY_KEY_TYPE
  #define FACTORY_KEY_TYPE_IS_DEFAULT
  #define FACTORY_KEY_TYPE const char*
#endif

// define FACTORY_KEY_COMPARATOR in your code if you want to change the comparator that is used for the map key.
// by default it will use cmp_cstr (defined in this header) as a comparison, or std::equal_to<T> if you've defined FACTORY_KEY_TYPE
#ifndef FACTORY_KEY_COMPARATOR
  #ifdef FACTORY_KEY_TYPE_IS_DEFAULT
    #define FACTORY_KEY_COMPARATOR cmp_cstr
  #else
    #define FACTORY_KEY_COMPARATOR std::equal_to<FACTORY_KEY_TYPE>
  #endif
#endif

// define FACTORY_USE_UNIQUEPTR if you want to have the factory return unique_ptr<T> instead of T*
// this also adds a shim for make_unique, for use in C++11, by default. you can define FACTORY_NO_MAKE_UNIQUE_SHIM if you don't want the shim.
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
class FACTORY_TYPE_NAME
{
  public:
    FACTORY_TYPE_NAME() = delete;

    // registration function. takes a unique key for the type, and a pointer to a function that creates an instance of the type.
    // FACTORY_T_PTR is usually T*, but it may be unique_ptr<T>
    static bool Register(FACTORY_KEY_TYPE key, FACTORY_T_PTR(*funcCreate)(TArgs...));

    // creates an instance of a type, by its key, with the provided constructor arguments.
    static FACTORY_T_PTR Create(FACTORY_KEY_TYPE key, TArgs... args);

    // returns true if a type is registered with a factory with the given key.
    static bool IsRegistered(FACTORY_KEY_TYPE key);

  private:
    // internal function that is used to get at the map, initialising it on first access.
    // this is how we avoid undefined behaviour with static member initialisation order >:)
    static std::map<FACTORY_KEY_TYPE, FACTORY_T_PTR(*)(TArgs...), FACTORY_KEY_COMPARATOR>* GetMap()
    {
      static std::map<FACTORY_KEY_TYPE, FACTORY_T_PTR(*)(TArgs...), FACTORY_KEY_COMPARATOR> registeredTypesInternal;
      return &registeredTypesInternal;
    }

  public:
    // get the number of types registered in the factory
    static size_t GetCount()
    {
      return GetMap()->size();
    }

    // get the key of the type at the given index
    // WARNING: this is O(n) on the index, so if you call this for every index in the factory this is O(n log n). this only matters in critical paths.
    // if you want to access by index instead of key, I recommend keeping your own map of the indices to keys.
    static FACTORY_KEY_TYPE GetKeyByIndex(size_t index)
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
bool FACTORY_TYPE_NAME<T, TArgs...>::Register(FACTORY_KEY_TYPE key, FACTORY_T_PTR(*funcCreate)(TArgs...))
{
  auto registeredTypes = GetMap();
  auto typeTuple = registeredTypes->find(key);
  if (typeTuple == registeredTypes->end())
  {
    (*registeredTypes)[key] = funcCreate;
    return true;
  }
  return false;
}

// create a type by its key
template<typename T, typename ...TArgs>
FACTORY_T_PTR FACTORY_TYPE_NAME<T, TArgs...>::Create(FACTORY_KEY_TYPE key, TArgs... args)
{
  auto registeredTypes = GetMap();
  auto typeTuple = registeredTypes->find(key);
  if (typeTuple != registeredTypes->end())
  {
    FACTORY_T_PTR(*funcCreate)(TArgs...) = typeTuple->second;
    return funcCreate(args...);
  }
  return nullptr;
}

// return true if the given type is registered
template<typename T, typename ...TArgs>
bool FACTORY_TYPE_NAME<T, TArgs...>::IsRegistered(FACTORY_KEY_TYPE key)
{
  auto registeredTypes = GetMap();
  auto typeTuple = registeredTypes->find(key);
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
bool RegisteredInFactory<TParent, TClass, TArgs...>::_FACTORY_INIT = FACTORY_TYPE_NAME<TParent, TArgs...>::Register(TClass::GetFactoryKey(), TClass::CreateInstance);

// if you've got one constructor, reference this in your class constructor to prevent the compiler from yeeting the initialiser and template during optimisation
#define FACTORY_INIT (void)_FACTORY_INIT;
// if you've got multiple constructors, reference this in each of your class constructors, using the same type parameters as you applied to RegisteredInFactory<...>
#define FACTORY_INIT_MANY(...) (void)(RegisteredInFactory<__VA_ARGS__>::_FACTORY_INIT);
