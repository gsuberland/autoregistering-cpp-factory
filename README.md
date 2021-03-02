# Auto-registering C++ factory

This is an auto-registering C++ factory, designed for C++11. It allows you to have classes register themselves to a factory, without needing to manually change the factory implementation each time you add a new type, or write a new factory for each base type. Constructor arguments are supported, as are overloaded constructors. Factory keys are typed as `const char*` by default, but this can be changed.

It has been tested on gcc and clang, both on desktop and in Arduino code, compiled with `-O2 -Werror -std=c++11 -pedantic-errors`.

## Motivation

I often run into cases where I'd much prefer to have a little extra code on each class instead of a switch statement in a factory somewhere. The compiler will generally shout at me if I mess the factory decoration up or `CreateInstance` / `GetFactoryKey` functions up, but it's much easier for me to forget to add a type to its factory. I also like the generic-ness of this approach, since I don't need to write a new factory class for each base type.

## Usage

Here's an example usage:

```cpp
#include "factory.h"

// a base type, for which we would like to create a factory
class Pet
{
protected:
	Pet() = default;
    Pet(int);
public:
    virtual ~Pet() = default;
    virtual const char* GetGreeting() = 0;
};

// first type being added to the factory
class Cat : public Pet, RegisteredInFactory<Pet, Cat, int>
{
public:
    Cat(int age)
    {
        FACTORY_INIT;
        printf("%d year old cat constructed.\n", age);
    }
    
    ~Cat() { }
    
    // if you would prefer to use unique_ptr as a return type here, simply
    // define FACTORY_USE_UNIQUEPTR before including factory.h
    static Pet* CreateInstance(int age)
    {
        return new Cat(age);
    }
    
    // the unique identifier of this type in the factory
    static const char* GetFactoryKey()
    {
        return "Cat";
    }
    
    const char* GetGreeting() override
    {
        return "Meow!";
    }
};

// another type being added to the factory
class Dog : public Pet, RegisteredInFactory<Pet, Dog, int>
{
public:
    Dog(int age)
    {
        FACTORY_INIT;
        printf("%d year old dog constructed.\n", age);
    }
    
    ~Dog() { }
    
    static Pet* CreateInstance(int age)
    {
        return new Dog(age);
    }
    
    static const char* GetFactoryKey()
    {
        return "Dog";
    }
    
    const char* GetGreeting() override
    {
        return "Woof!";
    }
};


int main()
{
    size_t registeredPetCount = Factory<Pet, int>::GetCount();
    for (size_t n = 0; n < registeredPetCount; n++)
    {
        const char* key = Factory<Pet, int>::GetKeyByIndex(n);
        printf("%s was registered as a pet.\n", key);
        int age = 5 + n;
        Pet* pet = Factory<Pet, int>::Create(key, age);
        printf("The %s says %s\n", key, pet->GetGreeting());
        delete pet;
    }
    return 0;
}
```

Output:

```
Cat was registered as a pet.
5 year old cat constructed.
The Cat says Meow!
Dog was registered as a pet.
6 year old dog constructed.
The Dog says Woof!
```

## API

The following APIs can be used to consume the factory:

```cpp
// create BaseType object by key (case sensitive)
BaseType* Factory<BaseType>::Create(const char* key);
// same as above, but with FACTORY_USE_UNIQUEPTR defined.
std::unique_ptr<BaseType> Factory<BaseType>::Create(const char* key);
// create BaseType object by key, using a constructor with arguments (again there's a unique_ptr version)
BaseType* Factory<BaseType, T1, T2, T3, ...>::Create(const char* key, T1 a, T2 b, T3 c, ...);

// get count of types registered in the factory
size_t Factory<BaseType>::GetCount();
size_t Factory<BaseType, T1, T2, T3, ...>::GetCount();

// get the key from the factory at the given index (useful for iterating types in the factory)
const char* Factory<BaseType>::GetKeyByIndex(size_t index);
const char* Factory<BaseType, T1, T2, T3, ...>::GetKeyByIndex(size_t index);

// check if a type is registered by key
bool Factory<BaseType>::IsRegistered(const char* key);
bool Factory<BaseType, T1, T2, T3, ...>::IsRegistered(const char* key);

// advanced: for dynamically adding things to the factory at runtime
bool Factory<BaseType>::Register(const char* key, T*(*funcCreate)());
bool Factory<BaseType>::Register(const char* key, unique_ptr<T>(*funcCreate)());
bool Factory<BaseType, T1, T2, T3, ...>::Register(const char* key, T*(*funcCreate)(T1,T2,T3,...));
bool Factory<BaseType, T1, T2, T3, ...>::Register(const char* key, unique_ptr<T>(*funcCreate)(T1,T2,T3,...));
```

The method for getting a class to register with a factory is as follows:

- Have it inherit from `RegisterWithFactory<TBase, TClass, ...>` where `TBase` is the base type for the factory, `TClass` is the class you're currently registering, and the remaining type parameters are the constructor argument types. For example, if your class is called `GZip`, its base is `ICompressionMethod`, and the constructor is `unsigned char*, size_t`, then you would inherit from `RegisterWithFactory<ICompressionMethod, GZip, unsigned char*, size_t> `.
- Reference `FACTORY_INIT` somewhere in your class, preferably in the constructor. This has zero runtime cost; it is just necessary to prevent the compiler's optimisation steps from eliminating the template.
- Include a public static `CreateInstance` method that creates a new instance of the class. Ensure that its parameters match the constructor.
- Include a public static `GetFactoryKey` method that returns the unique `const char*` key that is used to refer to the type in the factory. If this is not unique, only one type with that key will be added, and which one it will be is undefined.

Take a look at the example at the top of this document for reference.

You must include the exact same base type and constructor argument type list when referencing the factory. For example, if your class is registered as `RegisterWithFactory<ICompressionMethod, GZip, unsigned char*, size_t> `, the factory is referenced as `Factory<ICompressionMethod, unsigned char*, size_t>`.

If you've got a lot of constructor parameters, you may wish to alias the factory type:

```cpp
using PetFactory = Factory<Pet, int, const char*, Pet**, Person*>;
```

This allows you to use the API as `PetFactory::*` instead of needing to fully quality `Factory<...>` each time.

## Multiple constructors

If you've got multiple constructors on a class, you can overload the static `CreateInstance` method and use multiple inheritance to include matching `RegisterWithFactory<>` types for each constructor that you want to have accessible via the factory. You must then reference `FACTORY_INIT_MANY(...)` in each constructor, instead of just `FACTORY_INIT`, passing the same types as were used in the `RegisterWithFactory<...>` type. For example:

```cpp
class Dog : public Pet, RegisteredInFactory<Pet, Dog, int>, RegisteredInFactory<Pet, Dog, int, bool>
{
public:
    Dog(int age)
    {
        FACTORY_INIT_MANY(Pet, Dog, int);
        /* ... */
    }
    
    Dog(int age, bool hungry)
    {
        FACTORY_INIT_MANY(Pet, Dog, int, bool);
        /* ... */
    }
    
    static Pet* CreateInstance(int age)
    {
        return new Dog(age);
    }
    
    static Pet* CreateInstance(int age, bool hungry)
    {
        return new Dog(age, hungry);
    }
    
    /* ... */
};
```

This creates two separate factories, which may be accessed via `Factory<Pet, int>` and `Factory<Pet, int, bool>` respectively.

## Using unique_ptr

You can use `unique_ptr<T>` as the return type for your factory's create function by defining `FACTORY_USE_UNIQUEPTR` before including `factory.h`. This is disabled by default since my use-case is on embedded systems and I don't want the overhead of smart pointers, and also because `make_unique` is a C++14 feature and this was designed for C++11.

If you defined `FACTORY_USE_UNIQUEPTR`, a shim function called `make_unique` will be generated by default. This is to allow the feature to be used in C++11 if you want, albeit in a limited capacity (it uses `new` under the hood). If you don't want this shim, just define `FACTORY_NO_MAKE_UNIQUE_SHIM`.

## Changing the key type

By default the map key type is `const char*`, using a comparator of `cmp_cstr` which is implemented in the header, resulting in a case-sensitive ordinal comparison. You can change this by defining `FACTORY_KEY_TYPE` in your code, before including `factory.h`, setting its value to whatever key type you like.

If you define `FACTORY_KEY_TYPE`, the map equality comparison type will automatically be set to `std::equal_to<FACTORY_KEY_TYPE>`; if you want to override this behaviour and choose your own equality comparison you can do so by defining `FACTORY_KEY_COMPARATOR`.

For example, if you define `FACTORY_KEY_TYPE` to be `std::string`, then your `GetFactoryKey` methods must return `std::string` and all factory APIs will accept and return `std::string` types instead of the default `const char*`. The key equality comparator for the internal factory map will be set to `std::equal_to<std::string>`. Refer to `key_comp` in the [`std::map` reference](https://www.cplusplus.com/reference/map/map/map/) for details on the comparison predicate.

## Renaming the factory

If you've already got a type called `Factory` in your code, or you just want the factory class to have a different name for convenience, you can define `FACTORY_TYPE_NAME` before including `factory.h` in order to change the name of the factory type. For example, if you wrote `#define FACTORY_TYPE_NAME GenericAutoFactory` you would then use `GenericAutoFactory<...>` instead of `Factory<...>` in your code.

## Includes

No includes are added to the top of the file, with the exception of `<memory>` if `FACTORY_USE_UNIQUEPTR` is defined, since I use precompiled headers in my projects. The code only requires `<map>` and `<cstring>`. The latter is used only for the `cmp_cstr` comparison implementation that allows `const char*` strings to be used as keys in the map.

## How does this work?

Template magic, mostly. A static factory class is generated for each instance of `Factory<...>` you reference. A static `std::map` is created internally to store the type registrations, inside a static function to ensure that it is initialised at first access. At runtime startup a vestigial static field in the class is initialised via a static function that adds the type into the map. Since `FACTORY_INIT` is referenced in each type's constructor, this ensures that the vestigial static field is considered to be referenced, thus preventing the compiler from eliminating the initialisation logic and the factory template.

## Arduino

This was written with the intent of using it on Arduino. I don't know if the `std::map` implementation ends up being too chonky for smaller devices (e.g. Uno), but it should be small enough for your average ESP8266 or ESP32 device.

One feature you might want to be aware of is that, by default, when compiled for Arduino, an assertion failure will occur if you try to call `Factory<>::Create(key, ...)` with a key that does not exist in the map. I prefer being liberal with my use of `assert()` as it helps me catch bugs that would otherwise slip by me or be hard to debug, e.g. a random null pointer dereference due to an unchecked return value somewhere. If you would prefer to turn this off, define `FACTORY_NO_ARDUINO_ASSERT` before including the header. You do not need to worry about this on non-Arduino targets - the assert is only switched on for Arduino.

## Reference

The following links were useful while writing this:

- https://www.bfilipek.com/2018/02/factory-selfregister.html
- https://www.bfilipek.com/2018/02/static-vars-static-lib.html
- http://derydoca.com/2019/03/c-tutorial-auto-registering-factory/
- https://stackoverflow.com/questions/18701798/building-and-accessing-a-list-of-types-at-compile-time/18704609#18704609

## Thanks

Thanks to [Bartlomiej Filipek](https://www.bfilipek.com/) and [Derrick Canfield](http://derydoca.com/) for their blog posts on this topic.

Thanks to [Benji Smith](https://twitter.com/benjinsmith) and [@sdelang_asu](https://twitter.com/sdelang_asu) for helping out on Twitter.

## License

This code is released under [MIT license](LICENSE.txt). See the link for the full text of the license.

If you use this code in a public project or product, I would appreciate a mention :)