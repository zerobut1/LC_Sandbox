#pragma once

#include <luisa/core/stl/memory.h>

#include <rtweekend/hittable.h>
#include <rtweekend/hittable_list.h>
#include <rtweekend/material.h>
#include <rtweekend/sphere.h>

extern luisa::vector<luisa::unique_ptr<Yutrel::Material>> materials;