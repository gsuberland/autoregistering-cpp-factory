# Auto-registering C++ factory

This is an auto-registering C++ factory, designed for C++11. It allows you to have classes register themselves to a factory at compile-time. Constructor parameters are supported.

It has been tested on gcc and clang, both on desktop and in Arduino code, compiled with `-O2 -Werror -std=c++11 -pedantic-errors`.

Here's an example usage:

```cpp
#include "factory.h"

class Pet
{
protected:
	Pet() = default;
    Pet(int);
public:
    virtual ~Pet() = default;
    virtual const char* GetGreeting() = 0;
};

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
    
    static const char* GetFactoryKey()
    {
        return "Cat";
    }
    
    const char* GetGreeting() override
    {
        return "Meow!";
    }
};

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
        const char* name = Factory<Pet, int>::GetNameByIndex(n);
        printf("%s was registered as a pet.\n", name);
        int age = 5 + n;
        Pet* pet = Factory<Pet, int>::Create(name, age);
        printf("The %s says %s\n", name, pet->GetGreeting());
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
// create BaseType object by name
BaseType* Factory<BaseType>::Create(const char* name);
// same as above, but with FACTORY_USE_UNIQUEPTR defined.
std::unique_ptr<BaseType> Factory<BaseType>::Create(const char* name);
// create BaseType object by name, using a constructor with arguments (again there's a unique_ptr version)
BaseType* Factory<BaseType, T1, T2, T3, ...>::Create(const char* name, T1 a, T2 b, T3 c, ...);

// get count of types registered in the factory
size_t Factory<BaseType>::GetCount();
size_t Factory<BaseType, T1, T2, T3, ...>::GetCount();

// get name in the factory, by index
const char* Factory<BaseType>::GetNameByIndex(size_t index);
const char* Factory<BaseType, T1, T2, T3, ...>::GetNameByIndex(size_t index);

// check if a type is registered by name
bool Factory<BaseType>::IsRegistered(const char* name);
bool Factory<BaseType, T1, T2, T3, ...>::IsRegistered(const char* name);

// advanced: for dynamically adding things to the factory at runtime
bool Factory<BaseType>::Register(const char* name, T*(*funcCreate)());
bool Factory<BaseType>::Register(const char* name, unique_ptr<T>(*funcCreate)());
bool Factory<BaseType, T1, T2, T3, ...>::Register(const char* name, T*(*funcCreate)(T1,T2,T3,...));
bool Factory<BaseType, T1, T2, T3, ...>::Register(const char* name, unique_ptr<T>(*funcCreate)(T1,T2,T3,...));
```

The method for getting a class to register with a factory is as follows:

- Have it inherit from `RegisterWithFactory<TBase, TClass, ...>` where `TBase` is the base type for the factory, `TClass` is the class you're currently registering, and the remaining type parameters are the constructor argument types. For example, if your class is called `GZip`, its base is `ICompressionMethod` constructor is `unsigned char*, size_t` then you would inherit from `RegisterWithFactory<ICompressionMethod, GZip, unsigned char*, size_t> `.
- Reference `FACTORY_INIT` somewhere in your class, preferably in the constructor. This has zero runtime cost; it is just necessary to prevent the compiler's optimisation steps from eliminating the template.
- Include a public static `CreateInstance` method that creates a new instance of the class. Ensure that its parameters match the constructor.
- Include a public static `GetFactoryKey` method that returns the unique `const char*` key/name that is used to refer to the type in the factory. If this is not unique, only one type with that key will be added, and which one it will be is undefined.

Take a look at the example at the top of this document for reference.

If you've got a lot of constructor parameters, you may wish to alias the factory type:

```cpp
using PetFactory = Factory<Pet, int, const char*, Pet**, Person*>;
```

This allows you to use the API as `PetFactory::*` instead of needing to fully quality `Factory<...>` each time.

## Motivation

I often run into cases where I'd much prefer to have a little extra code on each class instead of a switch statement in a factory somewhere. The compiler will generally shout at me if I mess the factory decoration up or `CreateInstance` / `GetFactoryKey` functions up, but it's much easier for me to forget to add a type to its factory. I also like the generic-ness of this approach, since I don't need to write a new factory class for each base type.

## How does this work?

Template magic, mostly. A static factory class is generated for each instance of `Factory<...>` you reference. A static `std::map` is created internally to store the type registrations, inside a static function to ensure that it is initialised at first access. At runtime startup a vestigial static field in the class is initialised via a static function that adds the type into the map. Since `FACTORY_INIT` is referenced in each type's constructor, this ensures that the vestigial static field is considered to be referenced, thus preventing the compiler from eliminating the initialisation logic and the factory template.

## Using unique_ptr

You can use `unique_ptr<T>` as the return type for your factory's create function by defining `FACTORY_USE_UNIQUEPTR` before including `factory.h`. This is disabled by default since my use-case is on embedded systems and I don't want the overhead of smart pointers, and also because `make_unique` is a C++14 feature and this was designed for C++11.

If you defined `FACTORY_USE_UNIQUEPTR`, a shim function called `make_unique` will be generated by default. This is to allow the feature to be used in C++11 if you want, albeit in a limited capacity (it uses `new` under the hood). If you don't want this shim, just define `FACTORY_NO_MAKE_UNIQUE_SHIM`.

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