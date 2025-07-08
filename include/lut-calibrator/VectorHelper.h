#pragma once

/* VectorHelper.h
*
*  MIT License
*
*  Copyright (c) 2020-2025 awawa-dev
*
*  Project homesite: https://github.com/awawa-dev/HyperHDR
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
*/

#include <Eigen/Dense>
using Eigen::Vector2d;
using Eigen::Vector3d;
using Eigen::Vector4d;
using Eigen::Matrix3d;
using Eigen::Matrix4d;
using byte3 = Eigen::Matrix<uint8_t, 3, 1>;
using byte2 = Eigen::Matrix<uint8_t, 2, 1>;
using int3 = Eigen::Vector3i;
using int2 = Eigen::Vector2i;
using float2 = Eigen::Vector2f;
using float3 = Eigen::Vector3f;
using double2 = Eigen::Vector2d;
using double3 = Eigen::Vector3d;
using double4 = Eigen::Vector4d;
using double2x2 = Eigen::Matrix<double, 2, 2>;
using double3x3 = Eigen::Matrix<double, 3, 3>;
using double4x4 = Eigen::Matrix<double, 4, 4>;

