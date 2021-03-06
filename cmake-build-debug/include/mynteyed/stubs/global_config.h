// Copyright 2018 Slightech Co., Ltd. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef MYNTEYE_STUBS_GLOBAL_CONFIG_H_
#define MYNTEYE_STUBS_GLOBAL_CONFIG_H_
#pragma once

#define MYNTEYE_VERSION_MAJOR 1
#define MYNTEYE_VERSION_MINOR 6
#define MYNTEYE_VERSION_PATCH 0

/* MYNTEYE_VERSION is (major << 16) + (minor << 8) + patch */
#define MYNTEYE_VERSION \
MYNTEYE_VERSION_CHECK( \
  MYNTEYE_VERSION_MAJOR, \
  MYNTEYE_VERSION_MINOR, \
  MYNTEYE_VERSION_PATCH \
)

/* Can be used like
 *   #if (MYNTEYE_VERSION >= MYNTEYE_VERSION_CHECK(1, 0, 0)) */
#define MYNTEYE_VERSION_CHECK(major, minor, patch) \
  ((major<<16)|(minor<<8)|(patch))  // NOLINT

/* MYNTEYE_VERSION in "X.Y.Z" format */
#define MYNTEYE_VERSION_STR (MYNTEYE_STRINGIFY(MYNTEYE_VERSION_MAJOR.MYNTEYE_VERSION_MINOR.MYNTEYE_VERSION_PATCH))  // NOLINT

#define MYNTEYE_NAMESPACE mynteyed
#if defined(MYNTEYE_NAMESPACE)
# define MYNTEYE_BEGIN_NAMESPACE namespace MYNTEYE_NAMESPACE {
# define MYNTEYE_END_NAMESPACE }
# define MYNTEYE_USE_NAMESPACE using namespace ::MYNTEYE_NAMESPACE;  // NOLINT
#else
# define MYNTEYE_BEGIN_NAMESPACE
# define MYNTEYE_END_NAMESPACE
# define MYNTEYE_USE_NAMESPACE
#endif

#endif  // MYNTEYE_STUBS_GLOBAL_CONFIG_H_
