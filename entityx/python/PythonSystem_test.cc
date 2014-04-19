/*
 * Copyright (C) 2013 Alec Thomas <alec@swapoff.org>
 * All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.
 *
 * Author: Alec Thomas <alec@swapoff.org>
 */

 #define CATCH_CONFIG_MAIN

// NOTE: MUST be first include. See http://docs.python.org/2/extending/extending.html
#include <Python.h>
#include <boost/python.hpp>
#include <cassert>
#include <vector>
#include <string>
#include <iostream>
#include "entityx/python/3rdparty/catch.hpp"
#include "entityx/entityx.h"
#include "entityx/python/PythonSystem.h"

namespace py = boost::python;
using std::cerr;
using std::endl;
using namespace entityx;
using namespace entityx::python;


struct Position : public Component<Position> {
  Position(float x = 0.0, float y = 0.0) : x(x), y(y) {}

  float x, y;
};


struct Direction : public Component<Direction> {
  Direction(float x = 0.0, float y = 0.0) : x(x), y(y) {}

  float x, y;
};


struct CollisionEvent : public Event<CollisionEvent> {
  CollisionEvent(Entity a, Entity b) : a(a), b(b) {}

  Entity a, b;
};


struct CollisionEventProxy : public PythonEventProxy, public Receiver<CollisionEvent> {
  CollisionEventProxy() : PythonEventProxy("on_collision") {}

  void receive(const CollisionEvent &event) {
    for (auto entity : entities) {
      if (entity == event.a || entity == event.b) {
        auto py_entity = entity.component<PythonComponent>();
        py_entity->object.attr("on_collision")(event);
      }
    }
  }
};


BOOST_PYTHON_MODULE(entityx_python_test) {
  py::class_<Position, ptr<Position>>("Position", py::init<py::optional<float, float>>())
    .def("assign_to", &assign_to<Position>)
    .def("get_component", &get_component<Position>)
    .staticmethod("get_component")
    .def_readwrite("x", &Position::x)
    .def_readwrite("y", &Position::y);

  py::class_<Direction, ptr<Direction>>("Direction", py::init<py::optional<float, float>>())
    .def("assign_to", &assign_to<Direction>)
    .def("get_component", &get_component<Direction>)
    .staticmethod("get_component")
    .def_readwrite("x", &Direction::x)
    .def_readwrite("y", &Direction::y);

  py::class_<CollisionEvent, ptr<CollisionEvent>, py::bases<BaseEvent>>("Collision", py::init<Entity, Entity>())
    .add_property("a", py::make_getter(&CollisionEvent::a, py::return_value_policy<py::return_by_value>()))
    .add_property("b", py::make_getter(&CollisionEvent::b, py::return_value_policy<py::return_by_value>()));
}


class PythonSystemTest {
protected:
  PythonSystemTest() : event_manager(entityx::EventManager::make()), entity_manager(entityx::EntityManager::make(event_manager)) {
    assert(PyImport_AppendInittab("entityx_python_test", initentityx_python_test) != -1 && "Failed to initialize entityx_python_test Python module");
    python.reset(new PythonSystem(entity_manager));
    python->add_path(ENTITYX_PYTHON_TEST_DATA);
    if (!initialized) {
      initentityx_python_test();
      initialized = true;
    }
    python->add_event_proxy<CollisionEvent>(event_manager, ptr<CollisionEventProxy>(new CollisionEventProxy()));
    python->configure(event_manager);
  }

  ptr<PythonSystem> python;
  ptr<EventManager> event_manager;
  ptr<EntityManager> entity_manager;
  static bool initialized;
};

bool PythonSystemTest::initialized = false;


TEST_CASE_METHOD(PythonSystemTest, "TestSystemUpdateCallsEntityUpdate") {
  try {
    Entity e = entity_manager->create();
    auto script = e.assign<PythonComponent>("entityx.tests.update_test", "UpdateTest");
    REQUIRE(!py::extract<bool>(script->object.attr("updated")));
    python->update(entity_manager, event_manager, 0.1);
    REQUIRE(py::extract<bool>(script->object.attr("updated")));
  } catch(...) {
    PyErr_Print();
    PyErr_Clear();
    REQUIRE(false);
  }
}


TEST_CASE_METHOD(PythonSystemTest, "TestComponentAssignmentCreationInPython") {
  try {
    Entity e = entity_manager->create();
    auto script = e.assign<PythonComponent>("entityx.tests.assign_test", "AssignTest");
    REQUIRE(static_cast<bool>(e.component<Position>()));
    REQUIRE(script->object);
    REQUIRE(script->object.attr("test_assign_create"));
    script->object.attr("test_assign_create")();
    auto position = e.component<Position>();
    REQUIRE(static_cast<bool>(position));
    REQUIRE(position->x == 1.0);
    REQUIRE(position->y == 2.0);
  } catch(...) {
    PyErr_Print();
    PyErr_Clear();
    REQUIRE(false);
  }
}


TEST_CASE_METHOD(PythonSystemTest, "TestComponentAssignmentCreationInCpp") {
  try {
    Entity e = entity_manager->create();
    e.assign<Position>(2, 3);
    auto script = e.assign<PythonComponent>("entityx.tests.assign_test", "AssignTest");
    REQUIRE(static_cast<bool>(e.component<Position>()));
    REQUIRE(script->object);
    REQUIRE(script->object.attr("test_assign_existing"));
    script->object.attr("test_assign_existing")();
    auto position = e.component<Position>();
    REQUIRE(static_cast<bool>(position));
    REQUIRE(position->x == 3.0);
    REQUIRE(position->y == 4.0);
  } catch(...) {
    PyErr_Print();
    PyErr_Clear();
    REQUIRE(false);
  }
}


TEST_CASE_METHOD(PythonSystemTest, "TestEntityConstructorArgs") {
  try {
    Entity e = entity_manager->create();
    auto script = e.assign<PythonComponent>("entityx.tests.constructor_test", "ConstructorTest", 4.0, 5.0);
    auto position = e.component<Position>();
    REQUIRE(static_cast<bool>(position));
    REQUIRE(position->x == 4.0);
    REQUIRE(position->y == 5.0);
  } catch(...) {
    PyErr_Print();
    PyErr_Clear();
    REQUIRE(false);
  }
}


TEST_CASE_METHOD(PythonSystemTest, "TestEventDelivery") {
  try {
    Entity f = entity_manager->create();
    Entity e = entity_manager->create();
    Entity g = entity_manager->create();
    auto scripte = e.assign<PythonComponent>("entityx.tests.event_test", "EventTest");
    auto scriptf = f.assign<PythonComponent>("entityx.tests.event_test", "EventTest");
    auto scriptg = g.assign<PythonComponent>("entityx.tests.event_test", "EventTest");
    REQUIRE(!scripte->object.attr("collided"));
    REQUIRE(!scriptf->object.attr("collided"));
    event_manager->emit<CollisionEvent>(f, g);
    REQUIRE(scriptf->object.attr("collided"));
    REQUIRE(!scripte->object.attr("collided"));
    event_manager->emit<CollisionEvent>(e, f);
    REQUIRE(scriptf->object.attr("collided"));
    REQUIRE(scripte->object.attr("collided"));
  } catch(...) {
    PyErr_Print();
    PyErr_Clear();
    REQUIRE(false);
  }
}


TEST_CASE_METHOD(PythonSystemTest, "TestDeepEntitySubclass") {
  try {
    Entity e = entity_manager->create();
    auto script = e.assign<PythonComponent>("entityx.tests.deep_subclass_test", "DeepSubclassTest");
    REQUIRE(script->object.attr("test_deep_subclass"));
    script->object.attr("test_deep_subclass")();

    Entity e2 = entity_manager->create();
    auto script2 = e2.assign<PythonComponent>("entityx.tests.deep_subclass_test", "DeepSubclassTest2");
    REQUIRE(script2->object.attr("test_deeper_subclass"));
    script2->object.attr("test_deeper_subclass")();
  } catch(...) {
    PyErr_Print();
    PyErr_Clear();
    REQUIRE(false);
  }
}


TEST_CASE_METHOD(PythonSystemTest, "TestEntityCreationFromPython") {
  try {
    py::object test = py::import("entityx.tests.create_entities_from_python_test");
    test.attr("create_entities_from_python_test")();
  } catch(...) {
    PyErr_Print();
    PyErr_Clear();
    REQUIRE(false);
  }
}


TEST_CASE_METHOD(PythonSystemTest, "TestEventEmissionFromPython") {
  try {
    struct CollisionReceiver : public Receiver<CollisionReceiver> {
      void receive(const CollisionEvent &event) {
        a = event.a;
        b = event.b;
      }

      Entity a, b;
    };

    CollisionReceiver receiver;
    event_manager->subscribe<CollisionEvent>(receiver);

    REQUIRE(!receiver.a);
    REQUIRE(!receiver.b);

    py::object test = py::import("entityx.tests.event_emit_test");
    test.attr("emit_collision_from_python")();

    REQUIRE(receiver.a);
    REQUIRE(receiver.b);
  } catch(...) {
    PyErr_Print();
    PyErr_Clear();
    REQUIRE(false);
  }
}
