#ifndef ONEFLOW_CUSTOMIZED_KERNELS_CLIP_BY_VALUE_KERNEL_H_
#define ONEFLOW_CUSTOMIZED_KERNELS_CLIP_BY_VALUE_KERNEL_H_

#include "oneflow/core/framework/framework.h"
#include "oneflow/core/ndarray/xpu_util.h"

namespace oneflow {

template<DeviceType device_type, typename T>
struct ClipValuesUtil {
  static void ByMin(DeviceCtx* ctx, int64_t num_values, const T* values, const T* min_value,
                    T* out_ptr);
  static void ByMax(DeviceCtx* ctx, int64_t num_values, const T* values, const T* max_value,
                    T* out_ptr);
  static void ByMinMax(DeviceCtx* ctx, int64_t num_values, const T* values, const T* min_value,
                       const T* max_value, T* out_ptr);
};

template<DeviceType device_type, typename T>
struct ClipFunctor {
  OF_DEVICE_FUNC static T Min(const T value, const T min_value);
  OF_DEVICE_FUNC static T Max(const T value, const T max_value);
};

template<DeviceType device_type, typename T>
OF_DEVICE_FUNC void ClipValuesByMinMax(const int64_t num_values, const T* values, const T min_value,
                                       const T max_value, T* out_ptr) {
  XPU_1D_KERNEL_LOOP(i, num_values) {
    out_ptr[i] = ClipFunctor<device_type, T>::Min(
        ClipFunctor<device_type, T>::Max(values[i], min_value), max_value);
  }
}

template<DeviceType device_type, typename T>
OF_DEVICE_FUNC void ClipValuesByMin(const int64_t num_values, const T* values, const T min_value,
                                    T* out_ptr) {
  XPU_1D_KERNEL_LOOP(i, num_values) {
    out_ptr[i] = ClipFunctor<device_type, T>::Max(values[i], min_value);
  }
}

template<DeviceType device_type, typename T>
OF_DEVICE_FUNC void ClipValuesByMax(const int64_t num_values, const T* values, const T max_value,
                                    T* out_ptr) {
  XPU_1D_KERNEL_LOOP(i, num_values) {
    out_ptr[i] = ClipFunctor<device_type, T>::Min(values[i], max_value);
  }
}

template<DeviceType device_type, typename T>
class ClipByValueKernel final : public user_op::OpKernel {
 public:
  ClipByValueKernel(const user_op::KernelInitContext& ctx) : user_op::OpKernel(ctx) {}
  ClipByValueKernel() = default;
  ~ClipByValueKernel() = default;

 private:
  void Compute(user_op::KernelContext* ctx) override;
};

template<DeviceType device_type, typename T>
void ClipByValueKernel<device_type, T>::Compute(user_op::KernelContext* ctx) {
  const user_op::Tensor* in = ctx->Tensor4ArgNameAndIndex("in", 0);
  const user_op::Tensor* min = ctx->Tensor4ArgNameAndIndex("min", 0);
  const user_op::Tensor* max = ctx->Tensor4ArgNameAndIndex("max", 0);
  user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);

  if (in->dptr<T>() != out->mut_dptr<T>()) {
    size_t out_bytes_size = out->shape().elem_cnt() * GetSizeOfDataType(out->data_type());
    Memcpy<device_type>(ctx->device_ctx(), out->mut_dptr<T>(), in->dptr<T>(), out_bytes_size);
  }

  if (min != nullptr && max != nullptr) {
    ClipValuesUtil<device_type, T>::ByMinMax(ctx->device_ctx(), in->shape().elem_cnt(),
                                             in->dptr<T>(), min->dptr<T>(), max->dptr<T>(),
                                             out->mut_dptr<T>());
  } else if (min != nullptr) {
    ClipValuesUtil<device_type, T>::ByMin(ctx->device_ctx(), in->shape().elem_cnt(), in->dptr<T>(),
                                          min->dptr<T>(), out->mut_dptr<T>());
  } else if (max != nullptr) {
    ClipValuesUtil<device_type, T>::ByMax(ctx->device_ctx(), in->shape().elem_cnt(), in->dptr<T>(),
                                          max->dptr<T>(), out->mut_dptr<T>());
  } else {
    LOG(WARNING) << "min_value and max_value do not exist, and so values will not change.";
  }
}

#define REGISTER_CLIP_BY_VALUE_KERNEL(device_type_v, dtype_pair)                                \
  REGISTER_USER_KERNEL("clip_by_value")                                                         \
      .SetCreateFn([](const oneflow::user_op::KernelInitContext& ctx) {                         \
        return new ClipByValueKernel<device_type_v, OF_PP_PAIR_FIRST(dtype_pair)>(ctx);         \
      })                                                                                        \
      .SetIsMatchedPred([](const oneflow::user_op::KernelRegContext& ctx) {                     \
        const user_op::TensorDesc* out_desc = ctx.TensorDesc4ArgNameAndIndex("out", 0);         \
        if (ctx.device_type() == device_type_v                                                  \
            && out_desc->data_type() == OF_PP_PAIR_SECOND(dtype_pair)) {                        \
          return true;                                                                          \
        }                                                                                       \
        return false;                                                                           \
      })                                                                                        \
      .SetInplaceProposalFn([](const user_op::InferContext&,                                    \
                               user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> { \
        OF_RETURN_IF_ERROR(AddInplaceArgPairFn("out", 0, "in", 0, true));                       \
        return Maybe<void>::Ok();                                                               \
      });

#define INSTANTIATE_CLIP_VALUES_UTIL(device_type_v, dtype_pair)             \
  template struct ClipFunctor<device_type_v, OF_PP_PAIR_FIRST(dtype_pair)>; \
  template struct ClipValuesUtil<device_type_v, OF_PP_PAIR_FIRST(dtype_pair)>;

}  // namespace oneflow

#endif  // ONEFLOW_CUSTOMIZED_KERNELS_CLIP_BY_VALUE_KERNEL_H_
