// SPDX-FileCopyrightText: 2018 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#define KDECORATION_VERSION_CHECK(major, minor, patch, build) ((major<<24)|(minor<<16)|(patch<<8)|build)
#ifdef KDECORATION_VERSION_STR
#define KDECORATION_VERSION KDECORATION_VERSION_CHECK(KDECORATION_VERSION_MAJ, KDECORATION_VERSION_MIN, KDECORATION_VERSION_PAT, KDECORATION_VERSION_BUI)
#endif
