/*
 * Copyright (C) 2013 Alec Thomas <alec@swapoff.org>
 * All rights reserved.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution.
 *
 * Author: Alec Thomas <alec@swapoff.org>
 */

#pragma once

 // http://docs.python.org/2/extending/extending.html
#include <boost/python.hpp>
#include <boost/function.hpp>
#include <list>
#include <vector>
#include <string>
#include "entityx/System.h"
#include "entityx/Entity.h"
#include "entityx/Event.h"

namespace entityx {
namespace python {
/**
 * An EntityX component that represents a Python script.
 */
class PythonScript {
public:
  /**
   * Create a new PythonComponent from a Python Entity class.
   *
   * @param module The Python module where the Entity subclass resides.
   * @param cls The Class within the module. Must inherit from entityx.Entity.
   * @param args The *args to pass to the Python constructor.
   */
  template <typename ...Args>
  PythonScript(const std::string &module, const std::string &cls, Args ... args) : module(module), cls(cls) {
    unpack_args(args...);
  }

  /**
   * Create a new PythonComponent from an existing Python instance.
   */
  explicit PythonScript(boost::python::object object) : object(object) {}

  boost::python::object object;
  boost::python::list args;
  const std::string module, cls;

private:
  template <typename A, typename ...Args>
  void unpack_args(A &arg, Args ... remainder) {
    args.append(arg);
    unpack_args(remainder...);
  }

  void unpack_args() {}
};

class PythonSystem;

/**
 * Proxies C++ EntityX events to Python entities.
 */
class PythonEventProxy {
public:
  friend class PythonSystem;

  /**
   * Construct a new event proxy.
   *
   * @param handler_name The default implementation of can_send() tests for
   *     the existence of this attribute on an Entity.
   */
  explicit PythonEventProxy(const std::string &handler_name) : handler_name(handler_name) {}
  virtual ~PythonEventProxy() {}

  /**
   * Return true if this event can be sent to the provided Python entity.
   *
   * @param  object The Python entity to test for event delivery.
   */
  virtual bool can_send(const boost::python::object &object) const {
    return PyObject_HasAttrString(object.ptr(), handler_name.c_str());
  }

protected:
  std::list<Entity> entities;
  const std::string handler_name;

private:
  /**
   * Add an Entity receiver to this proxy. This is called automatically by PythonSystem.
   *
   * @param entity The entity that will receive events.
   */
  void add_receiver(Entity entity) {
    entities.push_back(entity);
  }

  /**
   * Delete an Entity receiver. This is called automatically by PythonSystem.
   *
   * @param entity The entity that was receiving events.
   */
  void delete_receiver(Entity entity) {
    for ( auto i = entities.begin(); i != entities.end(); ++i ) {
      if ( entity == *i ) {
        entities.erase(i);
        break;
      }
    }
  }
};

/**
 * A helper function for class_ to assign a component to an entity.
 */
template <typename Component>
void assign_to(Component& component, EntityManager& entity_manager, Entity::Id id) {
  entity_manager.assign<Component>(id, component);
}

/**
 * A helper function for retrieving an existing component associated with an
 * entity.
 */
template <typename Component>
static Component* get_component(EntityManager& em, Entity::Id id) {
  auto handle = em.component<Component>(id);
  if ( !handle )
    return NULL;
  return handle.get();
}

/**
 * A PythonEventProxy that broadcasts events to all entities with a matching
 * handler method.
 */
template <typename Event>
class BroadcastPythonEventProxy : public PythonEventProxy, public Receiver<BroadcastPythonEventProxy<Event>> {
public:
  BroadcastPythonEventProxy(const std::string &handler_name) : PythonEventProxy(handler_name) {}
  virtual ~BroadcastPythonEventProxy() {}

  void receive(const Event &event) {
    for ( auto entity : entities ) {
      auto py_entity = entity.template component<PythonComponent>();
      py_entity->object.attr(handler_name.c_str())(event);
    }
  }
};

/**
 * An entityx::System that bridges EntityX and Python.
 *
 * This system handles exposing entityx functionality to Python. The Python
 * support differs in design from the C++ design in the following ways:
 *
 * - Entities contain logic and can receive events.
 * - Systems and Components can not be implemented in Python.
 */
class PythonSystem : public entityx::System<PythonSystem>, public entityx::Receiver<PythonSystem> {
public:
  typedef std::function<void(const std::string &)> LoggerFunction;

  PythonSystem(EntityManager& entity_manager);  // NOLINT
  virtual ~PythonSystem();

  /**
   * Add system-installed entityx Python path to the interpreter.
   */
  void add_installed_library_path();

  /**
   * Add a Python path to the interpreter.
   */
  void add_path(const std::string &path);

  /**
   * Add a sequence of paths to the interpreter.
   */
  template <typename T>
  void add_paths(const T &paths) {
    for ( auto path : paths ) {
      add_path(path);
    }
  }

  /// Return the Python paths the system is configured with.
  const std::vector<std::string> &python_paths() const {
    return python_paths_;
  }

  virtual void configure(EventManager& event_manager) override;
  virtual void update(EntityManager& entities, EventManager& event_manager, TimeDelta dt) override;

  /**
   * Set line-based (not including \n) logger for stdout and stderr.
   */
  void log_to(LoggerFunction sout, LoggerFunction serr);

  /**
   * Proxy events of type Event to any Python entity with a handler_name method.
   */
  template <typename Event>
  void add_event_proxy(EventManager& event_manager, const std::string &handler_name) {
    std::shared_ptr<BroadcastPythonEventProxy<Event>> proxy(new BroadcastPythonEventProxy<Event>(handler_name));
    event_manager.subscribe<Event>(*proxy.get());
    event_proxies_.push_back(static_pointer_cast<PythonEventProxy>(proxy));
  }

  /**
   * Proxy events of type Event using the given PythonEventProxy implementation.
   */
  template <typename Event, typename Proxy>
  void add_event_proxy(EventManager& event_manager, std::shared_ptr<Proxy> proxy) {
    event_manager.subscribe<Event>(*proxy);
    event_proxies_.push_back(std::static_pointer_cast<PythonEventProxy>(proxy));
  }

  void receive(const EntityDestroyedEvent &event);
  void receive(const ComponentAddedEvent<PythonScript> &event);

private:
  void initialize_python_module();

  EntityManager& em_;
  std::vector<std::string> python_paths_;
  LoggerFunction stdout_, stderr_;
  static bool initialized_;
  std::vector<std::shared_ptr<PythonEventProxy>> event_proxies_;
};
}  // namespace python
}  // namespace entityx
