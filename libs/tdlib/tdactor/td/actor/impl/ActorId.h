//
// Copyright Aliaksei Levin (levlam@telegram.org), Arseny Smirnov (arseny30@gmail.com) 2014-2018
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "td/actor/impl/ActorId-decl.h"
#include "td/actor/impl/ActorInfo-decl.h"
#include "td/actor/impl/Scheduler-decl.h"

#include "td/utils/Slice.h"

namespace td {
/*** ActorId ***/

// If actor is on our scheduler(thread) result will be valid
// If actor is on another scheduler we will see it in migrate_dest_flags
template <class ActorType>
ActorInfo *ActorId<ActorType>::get_actor_info() const {
  if (ptr_.is_alive()) {
    return &*ptr_;
  }
  return nullptr;
}

template <class ActorType>
ActorType *ActorId<ActorType>::get_actor_unsafe() const {
  return static_cast<ActorType *>(ptr_->get_actor_unsafe());
}

template <class ActorType>
ActorType *ActorId<ActorType>::try_get_actor() const {
  auto info = get_actor_info();
  if (info && !info->is_migrating() && Scheduler::instance()->sched_id() == info->migrate_dest()) {
    return static_cast<ActorType *>(info->get_actor_unsafe());
  }
  return nullptr;
}

template <class ActorType>
Slice ActorId<ActorType>::get_name() const {
  return ptr_->get_name();
}

// ActorOwn
template <class ActorType>
ActorOwn<ActorType>::ActorOwn(ActorId<ActorType> id) : id_(std::move(id)) {
}
template <class ActorType>
template <class OtherActorType>
ActorOwn<ActorType>::ActorOwn(ActorId<OtherActorType> id) : id_(std::move(id)) {
}
template <class ActorType>
template <class OtherActorType>
ActorOwn<ActorType>::ActorOwn(ActorOwn<OtherActorType> &&other) : id_(other.release()) {
}
template <class ActorType>
template <class OtherActorType>
ActorOwn<ActorType> &ActorOwn<ActorType>::operator=(ActorOwn<OtherActorType> &&other) {
  reset(static_cast<ActorId<ActorType>>(other.release()));
  return *this;
}

template <class ActorType>
ActorOwn<ActorType>::ActorOwn(ActorOwn &&other) : id_(other.release()) {
}
template <class ActorType>
ActorOwn<ActorType> &ActorOwn<ActorType>::operator=(ActorOwn &&other) {
  reset(other.release());
  return *this;
}

template <class ActorType>
ActorOwn<ActorType>::~ActorOwn() {
  reset();
}

template <class ActorType>
bool ActorOwn<ActorType>::empty() const {
  return id_.empty();
}
template <class ActorType>
ActorId<ActorType> ActorOwn<ActorType>::get() const {
  return id_;
}

template <class ActorType>
ActorId<ActorType> ActorOwn<ActorType>::release() {
  return std::move(id_);
}
template <class ActorType>
void ActorOwn<ActorType>::reset(ActorId<ActorType> other) {
  static_assert(sizeof(ActorType) > 0, "Can't use ActorOwn with incomplete type");
  hangup();
  id_ = std::move(other);
}

template <class ActorType>
void ActorOwn<ActorType>::hangup() const {
  if (!id_.empty()) {
    send_event(id_, Event::hangup());
  }
}
template <class ActorType>
const ActorId<ActorType> *ActorOwn<ActorType>::operator->() const {
  return &id_;
}

// ActorShared
template <class ActorType>
template <class OtherActorType>
ActorShared<ActorType>::ActorShared(ActorId<OtherActorType> id, uint64 token) : id_(std::move(id)), token_(token) {
}
template <class ActorType>
template <class OtherActorType>
ActorShared<ActorType>::ActorShared(ActorShared<OtherActorType> &&other) : id_(other.release()), token_(other.token()) {
}
template <class ActorType>
template <class OtherActorType>
ActorShared<ActorType>::ActorShared(ActorOwn<OtherActorType> &&other) : id_(other.release()), token_(0) {
}
template <class ActorType>
template <class OtherActorType>
ActorShared<ActorType> &ActorShared<ActorType>::operator=(ActorShared<OtherActorType> &&other) {
  reset(other.release());
  token_ = other.token();
  return *this;
}

template <class ActorType>
ActorShared<ActorType>::ActorShared(ActorShared &&other) : id_(other.release()), token_(other.token_) {
}
template <class ActorType>
ActorShared<ActorType> &ActorShared<ActorType>::operator=(ActorShared &&other) {
  reset(other.release());
  token_ = other.token_;
  return *this;
}

template <class ActorType>
ActorShared<ActorType>::~ActorShared() {
  reset();
}

template <class ActorType>
uint64 ActorShared<ActorType>::token() const {
  return token_;
}
template <class ActorType>
bool ActorShared<ActorType>::empty() const {
  return id_.empty();
}
template <class ActorType>
ActorId<ActorType> ActorShared<ActorType>::get() const {
  return id_;
}

template <class ActorType>
ActorId<ActorType> ActorShared<ActorType>::release() {
  return std::move(id_);
}
template <class ActorType>
void ActorShared<ActorType>::reset(ActorId<ActorType> other) {
  reset<ActorType>(std::move(other));
}

template <class ActorType>
template <class OtherActorType>
void ActorShared<ActorType>::reset(ActorId<OtherActorType> other) {
  static_assert(sizeof(ActorType) > 0, "Can't use ActorShared with incomplete type");
  if (!id_.empty()) {
    send_event(*this, Event::hangup());
  }
  id_ = static_cast<ActorId<ActorType>>(other);
}
template <class ActorType>
const ActorId<ActorType> *ActorShared<ActorType>::operator->() const {
  return &id_;
}

/*** ActorRef ***/
template <class T>
ActorRef::ActorRef(const ActorId<T> &actor_id) : actor_id_(actor_id) {
}
template <class T>
ActorRef::ActorRef(const ActorShared<T> &actor_id) : actor_id_(actor_id.get()), token_(actor_id.token()) {
}
template <class T>
ActorRef::ActorRef(ActorShared<T> &&actor_id) : actor_id_(actor_id.release()), token_(actor_id.token()) {
}
template <class T>
ActorRef::ActorRef(const ActorOwn<T> &actor_id) : actor_id_(actor_id.get()) {
}
template <class T>
ActorRef::ActorRef(ActorOwn<T> &&actor_id) : actor_id_(actor_id.release()) {
}

}  // namespace td
