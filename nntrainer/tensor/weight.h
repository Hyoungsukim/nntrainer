// SPDX-License-Identifier: Apache-2.0
/**
 * Copyright (C) 2020 Parichay Kapoor <pk.kapoor@samsung.com>
 *
 * @file   weight.h
 * @date   22 September 2020
 * @see    https://github.com/nnstreamer/nntrainer
 * @author Parichay Kapoor <pk.kapoor@samsung.com>
 * @bug    No known bugs except for NYI items
 * @brief  This is Weight Class for Neural Network
 *
 */

#ifndef __WEIGHT_H__
#define __WEIGHT_H__

#include <tuple>

#include <tensor.h>
#include <tensor_wrap_specs.h>
#include <var_grad.h>

namespace nntrainer {

/**
 * @class   Weight
 * @brief   Weight extends over Var_Grad with regularization & optimizer updates
 */
class Weight : public Var_Grad {
public:
  /**
   * @brief Specification of the Weight
   *
   * @details The tuple values are dimension, initializer, regularizer,
   * regularizer_constant, need_gradient property amd name of the Weight object.
   */
  typedef WeightSpec Spec;

  /**
   * @brief Weight default constructor
   */
  Weight() :
    Var_Grad(),
    regularizer(WeightRegularizer::UNKNOWN),
    regularizer_constant(1.0f) {}

  /**
   * @brief Construct a new Weight object
   *
   * @param dim Variable and gradient tensor dimension
   * @param init Initializer for the weight
   * @param reg Regularizer for the weight
   * @param reg_const Constant multiplier for regularizer
   * @param ng If the variable needs gradient
   * @param alloc_now The memory for the weight tensors be allocated upon init
   * @param name Name for this weight
   */
  explicit Weight(
    const TensorDim &dim,
    const Tensor::Initializer init = Tensor::Initializer::XAVIER_UNIFORM,
    const WeightRegularizer reg = WeightRegularizer::NONE,
    const float reg_const = 1.0f, bool ng = true, bool alloc_now = false,
    std::string name = "");

  /**
   * @brief Construct a new Weight object
   *
   * @param spec Weight specification
   */
  explicit Weight(const Spec &spec, bool alloc_now = false) :
    Weight(std::get<0>(spec), // TensorDim
           std::get<1>(spec), // Tensor::Initializer
           std::get<2>(spec), // WeightRegularizer
           std::get<3>(spec), // WeightRegularizerConstant
           std::get<4>(spec), // need_gradient
           alloc_now,
           std::get<5>(spec) // Name
    ) {}

  /**
   * @brief Construct a new Weight object
   *
   * @param v Already created variable object
   * @param g Already created gradient object
   * @param n Name for this Weight
   *
   * @note This is primarily used to created wrapper of variable extracted from
   * context. If needed, add support for regularizer, and opt_vars.
   *
   * @note This API is not recommended for usage and must be used for internal
   * uses only, as Weight does not own the tensors v and g, and can go invalid
   * if the owner of these tensors free the tensors.
   */
  explicit Weight(const Tensor &v, const Tensor &g, const std::string &n = "") :
    Var_Grad(v, g, n),
    regularizer(WeightRegularizer::NONE),
    regularizer_constant(1.0f) {}

  /**
   * @copydoc var_grad::initializeGradient(const Tensor &)
   */
  void initializeGradient(const Tensor &preallocated = Tensor());

  /**
   * @brief Swap for weight
   *
   * @param lhs Swap to
   * @param rhs Swap from
   * @note Only swap gradient if need gradient
   */
  friend void swap(Weight &lhs, Weight &rhs) noexcept {
    using std::swap;
    swap(static_cast<Var_Grad &>(lhs), static_cast<Var_Grad &>(rhs));
    swap(lhs.regularizer, rhs.regularizer);
  }

  /**
   * @brief Copy constructor for weight
   *
   * @param rhs weight to construct from
   */
  Weight(const Weight &rhs) = default;

  /**
   * @brief Move constructor for weight
   *
   * @param rhs weight to construct from
   */
  Weight(Weight &&rhs) = default;

  /**
   * @brief copy assigment
   *
   * @param rhs copy from
   * @return Weight& Updated weight
   */
  Weight &operator=(const Weight &rhs) = default;

  /**
   * @brief move assignment
   *
   * @param rhs move from
   * @return Weight& Updated weight
   */
  Weight &operator=(Weight &&rhs) = default;

  /**
   * @brief Clone the currnet object
   *
   * @return Cloned copy
   */
  Weight clone() const {
    Weight w(*this);
    if (!this->var->empty())
      w.var = std::make_shared<Tensor>(this->var->clone());
    if (!this->grad->empty())
      w.grad = std::make_shared<Tensor>(this->grad->clone());

    return w;
  }

  /**
   * @brief Reset the weight
   *
   * @param dim Variable and gradient tensor dimension
   * @param init Initializer for the weight
   * @param reg Regularizer for the weight
   * @param ng If the variable needs gradient
   *
   * @note New dimension must maintain the shape of the variable
   */
  void reset(const TensorDim &dim, const Tensor::Initializer init,
             const WeightRegularizer reg, const float reg_const, bool ng) {
    regularizer = reg;
    regularizer_constant = reg_const;

    Var_Grad::reset(dim, init, ng);
  }

  /**
   * @brief Clear optimizer variables
   */
  void clearOptimizerVariables() {
    opt_vars.clear();
    opt_vars_dim.clear();
  }

  /**
   * @brief Add optimizer variables
   * @param dim Optimizer variable dimension
   */
  void addOptimizerVariable(const TensorDim &dim) {
    opt_vars_dim.emplace_back(dim);
    // TODO: Move this out when an optimizer does not initialize with 0.
  }

  /**
   * @brief Get optimizer variable reference
   * @param idx Index of the optimizer variable to get
   * @retval Reference of the optimizer variable
   */
  Tensor &getOptimizerVariableRef(unsigned int idx) { return opt_vars[idx]; }

  /**
   * @brief Allocate and initialize the weight variable, if needed
   */
  void allocateVariable() { Var_Grad::allocateVariable(); }

  /**
   * @brief Allocate and initialize the weight gradient, if needed
   */
  void allocateGradient() {
    Var_Grad::allocateGradient();
    allocateOptimizerVariables();
  }

  /**
   * @brief     check if weight regularizer type is l2norm
   * @return    bool is weight regrulatizer type is L2 Norm
   */
  bool isWeightRegularizerL2Norm() {
    return regularizer == WeightRegularizer::L2NORM;
  }

  /**
   * @brief     Get loss from the regularization of the weight
   */
  float getRegularizationLoss() {
    if (hasGradient() && isWeightRegularizerL2Norm())
      return regularizer_constant * 0.5f * var->l2norm();

    return 0;
  }

  /**
   * @brief     Calculate gradient from the regularizaiton of the weight
   */
  void calcRegularizationGradient() {
    if (isWeightRegularizerL2Norm())
      grad->add_i(*var.get(), regularizer_constant);
  }

  /**
   * @brief     Apply the gradient to the weight
   */
  void applyGradient(double lr) { var->add_i(*grad.get(), -lr); }

  /**
   * @brief Deallocate memory for the gradient of the weight
   */
  void deallocateGradient() {
    Var_Grad::deallocateGradient();
    opt_vars.clear();
  }

  /**
   * @brief Deallocate the weight gardient and variable
   */
  void deallocate() {
    deallocateGradient();
    deallocateVariable();
  }

private:
  WeightRegularizer regularizer; /**< regularizer for this variable */
  float regularizer_constant;    /**< constant factor for regularization */

  std::vector<Tensor> opt_vars;        /**< optimizer variables */
  std::vector<TensorDim> opt_vars_dim; /**< optimizer variables dimensions */

  /**
   * @brief Allocate optimizer related variables for the given weights
   */
  void allocateOptimizerVariables();
};

} // namespace nntrainer

#endif /** __WEIGHT_H__ */
