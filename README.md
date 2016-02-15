# Python Bindings for [EntityX](https://github.com/alecthomas/entityx) (α Alpha)

This system adds the ability to extend entity logic with Python scripts. The goal is to allow ad-hoc behaviour to be assigned to entities through scripts, in contrast to the more strictly pure entity-component system approach.

## Example

```python
from entityx import Entity, Component, emit
from mygame import Position, Health, Dead


class Player(Entity):
    position = Component(Position, 0, 0)
    health = Component(Health, 100)

    def on_collision(self, event):
        self.health.health -= 10
        if self.health.health <= 0:
            emit(Dead(self))

```

## Building and installing

EntityX Python has the following build and runtime requirements:

- [EntityX](https://github.com/alecthomas/entityx)
- [Boost Python](https://boostorg.github.io/python/doc/html/index.html)
- [CMake](http://cmake.org/)

### CMake Options:

- `ENTITYX_PYTHON_BUILD_TESTING` : Enable building of tests
- `BOOST_ROOT` : Set path to boost root if CMake did not find it
- `ENTITYX_ROOT` : Set path to EntityX root if CMake did not find it
- `PYTHON_ROOT` : Set path to Python root if CMake did not find it

Check out the source to entityx_python, and run:

```bash
mkdir build && cd build
cmake  ..
make
make install
```

## Design

- Python scripts are attached to entities via `PythonScript`.
- Systems and components can not be created from Python, primarily for performance reasons.
- Events are proxied directly to Python entities via `PythonEventProxy` objects.
    - Each event to be handled in Python must have an associated `PythonEventProxy`implementation.
    - As a convenience `BroadcastPythonEventProxy<Event>(handler_method)` can be used. It will broadcast events to all `PythonScript` entities with a `<handler_method>`.
- `PythonSystem` manages scripted entity lifecycle and event delivery.

## Summary

To add scripting support to your system, something like the following steps should be followed:

1. Expose C++ `Component` and `Event` classes to Python with `BOOST_PYTHON_MODULE`.
2. Initialize the module with `PyImport_AppendInittab`.
3. Create a Python package.
4. Add classes to the package, inheriting from `entityx.Entity` and using the `entityx.Component` descriptor to assign components.
5. Create a `PythonSystem`, passing in the list of paths to add to Python's import search path.
6. Optionally attach any event proxies.
7. Create an entity and associate it with a Python script by assigning `PythonScript`, passing it the package name, class name, and any constructor arguments.
8. When finished, call `EntityManager::destroy_all()`.

## Interfacing with Python

`entityx::python` primarily uses standard `boost::python` to interface with Python, with some helper classes and functions.

### Exposing C++ Components to Python

In most cases, this should be pretty simple. Given a component, provide a `boost::python` class definition with two extra methods defined with EntityX::Python helper functions `assign_to<Component>` and `get_component<Component>`. These are used from Python to assign Python-created components to an entity and to retrieve existing components from an entity, respectively.

Here's an example:

```c++
namespace py = boost::python;

struct Position : public entityx::Component<Position> {
  Position(float x = 0.0, float y = 0.0) : x(x), y(y) {}

  float x, y;
};

void export_position_to_python() {
  py::class_<Position>("Position", py::init<py::optional<float, float>>())
    // Allows this component to be assigned to an entity
    .def("assign_to", &entityx::python::assign_to<Position>)
    // Allows this component to be retrieved from an entity.
    // Set return_value_policy to reference raw component pointer
    .def("get_component", &entityx::python::get_component<Position>,
         py::return_value_policy<py::reference_existing_object>() )
    .staticmethod("get_component")
    .def_readwrite("x", &Position::x)
    .def_readwrite("y", &Position::y);
}

BOOST_PYTHON_MODULE(mygame) {
  export_position_to_python();
}
```

### Using C++ Components from Python

Use the `entityx.Component` class descriptor to associate components and provide default constructor arguments:

```python
import entityx
from mygame import Position  # C++ Component

class MyEntity(entityx.Entity):
    # Ensures MyEntity has an associated Position component,
    # constructed with the given arguments.
    position = entityx.Component(Position, 1, 2)

    def __init__(self):
        assert self.position.x == 1
        assert self.position.y == 2
```

### Delivering events to Python entities

Unlike in C++, where events are typically handled by systems, EntityX::Python
explicitly provides support for sending events to entities. To bridge this gap
use the `PythonEventProxy` class to receive C++ events and proxy them to
Python entities.

The class takes a single parameter, which is the name of the attribute on a
Python entity. If this attribute exists, the entity will be added to
`PythonEventProxy::entities (std::list<Entity>)`, so that matching entities
will be accessible from any event handlers.

This checking is performed in `PythonEventProxy::can_send()`, and can be
overridden, but further checking can also be done in the event `receive()`
method.

A helper template class called `BroadcastPythonEventProxy<Event>` is provided
that will broadcast events to any entity with the corresponding handler method.

To implement more refined logic, subclass `PythonEventProxy` and operate on
the protected member `entities`. Here's a collision example, where the proxy
only delivers collision events to the colliding entities themselves:

```c++
struct CollisionEvent : public entityx::Event<CollisionEvent> {
  CollisionEvent(Entity a, Entity b) : a(a), b(b) {}

  // NOTE: See note below in export_collision_event_to_python().
  Entity a, b;
};

struct CollisionEventProxy : public entityx::python::PythonEventProxy, public entityx::Receiver<CollisionEvent> {
  CollisionEventProxy() : entityx::python::PythonEventProxy("on_collision") {}

  void receive(const CollisionEvent &event) {
    // "entities" is a protected data member, populated by
    // PythonSystem, with Python entities that pass can_send().
    for (auto entity : entities) {
      auto py_entity = entity.template component<entityx::python::PythonComponent>();
      if (entity == event.a || entity == event.b) {
        py_entity->object.attr(handler_name.c_str())(event);
      }
    }
  }
};

void export_collision_event_to_python() {
  py::class_<CollisionEvent>("Collision", py::init<Entity, Entity>())
    // NOTE: Normally, def_readonly() would be used to expose attributes,
    // but you must use the following construct in order for Entity
    // objects to be automatically converted into their Python instances.
    .add_property("a", py::make_getter(&CollisionEvent::a, py::return_value_policy<py::return_by_value>()))
    .add_property("b", py::make_getter(&CollisionEvent::b, py::return_value_policy<py::return_by_value>()));

  // Register event manager emit so signal handlers will trigger properly
  void (EventManager::*emit)(const CollisionEvent &) = &EventManager::emit;

  py::class_<EventManager, boost::noncopyable>("EventManager", py::no_init)
    .def("emit", emit);
}


BOOST_PYTHON_MODULE(mygame) {
  export_position_to_python();
  export_collision_event_to_python();
}
```


### Sending events from Python

This is relatively straight forward. Once you have exported a C++ event to Python:

```python
from entityx import Entity, emit
from mygame import Collision


class AnEntity(Entity): pass


emit(Collision(AnEntity(), AnEntity()))
```


### Initialization

Finally, initialize the `mygame` module once, before using `PythonSystem`, with something like this:

```c++
// This should only be performed once, at application initialization time.
CHECK(PyImport_AppendInittab("mygame", initmygame) != -1)
  << "Failed to initialize mygame Python module";
```

Then create a `PythonSystem` as necessary:

```c++
// Initialize the PythonSystem.
vector<string> paths;
// Ensure that MYGAME_PYTHON_PATH includes entityx.py from this distribution.
paths.push_back(MYGAME_PYTHON_PATH);
// +any other Python paths...
entityx::python::PythonSystem python(paths);

// Add any Event proxies.
python->add_event_proxy<CollisionEvent>(ev, std::make_shared<CollisionEventProxy>());
```
