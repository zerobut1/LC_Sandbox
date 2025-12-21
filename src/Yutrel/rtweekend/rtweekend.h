#pragma once

#include <luisa/core/stl/memory.h>

#include <rtweekend/bvh.h>
#include <rtweekend/hittable.h>
#include <rtweekend/hittable_list.h>
#include <rtweekend/material.h>
#include <rtweekend/sphere.h>
#include <rtweekend/utils.h>

extern luisa::vector<luisa::unique_ptr<Yutrel::RTWeekend::Material>> materials;