#include "scenario/monopod/Joint.h"
#include "scenario/monopod/Model.h"
#include "scenario/monopod/World.h"

#include <cassert>
#include <stdexcept>
#include <limits>

#include "scenario/monopod/easylogging++.h"

using namespace scenario::monopod;
const scenario::core::PID DefaultPID;

class Joint::Impl
{
public:
    // We only have Revolute joints
    const core::JointType jointType = core::JointType::Revolute;
    core::JointControlMode jointControlMode = core::JointControlMode::Idle;
    std::optional<std::vector<double>> jointMaxGeneralizedForce;
    std::string parentModelName;
    std::string name;
    int monopodSdkIndex;
    std::shared_ptr<monopod_drivers::Monopod> monopod_sdk;
};

Joint::Joint()
    : pImpl{std::make_unique<Impl>()}
{}

Joint::~Joint() = default;

uint64_t Joint::id() const
{
    // Build a unique string identifier of this joint
    const std::string scopedJointName =
        pImpl->parentModelName + "::" + this->name(/*scoped=*/true);

    // Return the hashed string
    return std::hash<std::string>{}(scopedJointName);
}

bool Joint::initialize(const std::pair<std::string, int> nameIndexPair,
                       const std::shared_ptr<monopod_drivers::Monopod> &monopod_sdk)
{
    // Set the names...
    pImpl->name = nameIndexPair.first;
    pImpl->monopodSdkIndex = nameIndexPair.second;
    pImpl->monopod_sdk = monopod_sdk;
    pImpl->parentModelName = pImpl->monopod_sdk->get_model_name();

    if (this->dofs() > 1) {
        LOG(ERROR) << "Joints with DoFs > 1 are not currently supported";
        return false;
    }
    // Todo::Set default PID here.
    return true;
}

bool Joint::valid() const
{
    // Hardware check here...
    bool ok = true;
    return ok;
}

scenario::core::JointType Joint::type() const
{
    return pImpl->jointType;
}

size_t Joint::dofs() const
{
    switch (this->type()) {
        case core::JointType::Fixed:
        case core::JointType::Revolute:
        case core::JointType::Prismatic:
            return 1;
        case core::JointType::Invalid:
            return 0;
        default:
            assert(false);
            return 0;
    }
}

std::string Joint::name(const bool scoped) const
{
    std::string jointName;
    if (scoped) {
        jointName = pImpl->parentModelName + "::" + pImpl->name;
    }else{
        jointName = pImpl->name;
    }
    return jointName;
}

bool Joint::setControlMode(const scenario::core::JointControlMode mode)
{
    // Real robot only has torque control or invalid (no control)
    switch (mode) {
        case core::JointControlMode::Force:
        {
            pImpl->jointControlMode = mode;
            // Set the force targets to 0 for all DOF...
            std::vector<double> forcetarget(this->dofs(), 0);
            this->setJointGeneralizedForceTarget(forcetarget);
            return true;
        }
        default:
            LOG(ERROR) << "Only support force control mode.";
            return false;
    }
    return false;
}

scenario::core::JointControlMode Joint::controlMode() const
{
    return pImpl->jointControlMode;
}

std::vector<double> Joint::jointPosition() const
{
    std::vector<double> jointPosition;
    jointPosition.reserve(1);
    auto data = pImpl->monopod_sdk->get_position(pImpl->monopodSdkIndex);
    if(data.has_value())
        jointPosition.push_back(data.value());

    if (jointPosition.size() != this->dofs()) {
        LOG(ERROR) << "The size of velocity being set for the joint '"
                   + this->name() + "' does not match the joint's DOFs.";
    }

    LOG(INFO) << "Getting position for joint, " + this->name() + " = " + std::to_string(data.value());

    return jointPosition;
}

std::vector<double> Joint::jointVelocity() const
{
    std::vector<double> jointVelocity;
    jointVelocity.reserve(1);
    auto data = pImpl->monopod_sdk->get_velocity(pImpl->monopodSdkIndex);
    if(data.has_value())
        jointVelocity.push_back(data.value());


    if (jointVelocity.size() != this->dofs()) {
        LOG(ERROR) << "The size of velocity being set for the joint '"
                    + this->name() + "' does not match the joint's DOFs.";
    }

    LOG(INFO) << "Getting velocity for joint, " + this->name() + " = " + std::to_string(data.value());

    return jointVelocity;
}

std::vector<double> Joint::jointAcceleration() const
{
    std::vector<double> jointAcceleration;
    jointAcceleration.reserve(1);
    auto data = pImpl->monopod_sdk->get_acceleration(pImpl->monopodSdkIndex);
    if(data.has_value())
        jointAcceleration.push_back(data.value());

    if (jointAcceleration.size() != this->dofs()) {
        LOG(ERROR) << "The size of acceleration being set for the joint '"
                   + this->name() + "' does not match the joint's DOFs.";
    }

    LOG(INFO) << "Getting acceleration for joint, " + this->name() + " = " + std::to_string(data.value());

    return jointAcceleration;
}

std::vector<double> Joint::jointGeneralizedForceTarget() const
{
    std::vector<double> torque_target;
    torque_target.reserve(1);
    auto data = pImpl->monopod_sdk->get_torque_target(pImpl->monopodSdkIndex);
    if(data.has_value())
        torque_target.push_back(data.value());

    return torque_target;
}

bool Joint::setJointGeneralizedForceTarget(const std::vector<double>& force)
{
    if (force.size() != this->dofs()) {
        LOG(ERROR) << "Wrong number of elements (joint_dofs=" + std::to_string(this->dofs()) + ")";
        return false;

    }

    const std::vector<double>& maxForce = this->jointMaxGeneralizedForce();
    for (size_t dof = 0; dof < this->dofs(); ++dof) {
        if (std::abs(force[dof]) > maxForce[dof]) {
            LOG(WARNING) << "The force target is higher than the limit. Will be clipped.";
        }
    }

    // Print values for testing
    std::string msg = "Setting the joint, " + this->name() + ", to the force value: ";
    for (auto i: force)
        msg = msg + std::to_string(i) + ", ";
    LOG(INFO) << msg;

    switch (this->controlMode()) {
        case core::JointControlMode::Force:
            // Set the component data
            return pImpl->monopod_sdk->set_torque_target(force[0], pImpl->monopodSdkIndex);
        default:
            LOG(ERROR) << "Joint, '" + this->name() + "' is not in force control mode.";
            return false;
    }
}

std::vector<double> Joint::jointMaxGeneralizedForce() const
{
    // if(!pImpl->jointMaxGeneralizedForce.has_value()){
    //     // Set to default value here.
    //     std::vector<double> defaultMaxForce(this->dofs(), std::numeric_limits<double>::infinity());
    //     pImpl->jointMaxGeneralizedForce = defaultMaxForce;
    // }
    //
    // std::vector<double> maxGeneralizedForce(this->dofs(), 0);
    // switch (this->type()) {
    //     case scenario::core::JointType::Revolute: {
    //         maxGeneralizedForce = pImpl->jointMaxGeneralizedForce.value();
    //         break;
    //     }
    //     default: {
    //         LOG(WARNING) << "Type of Joint with name '" + this->name()
    //                       + "' has no max effort defined. Only defined for Revolute Joints";
    //         break;
    //     }
    // }
    // return maxGeneralizedForce;

    auto data = pImpl->monopod_sdk->get_max_torque_target(pImpl->monopodSdkIndex);

    std::vector<double> maxGeneralizedForce;
    maxGeneralizedForce.reserve(1);

    if(data.has_value()){
      maxGeneralizedForce.push_back(data.value());
        // Set to default value here.
    }else{
      maxGeneralizedForce.push_back(0);
    }
    return maxGeneralizedForce;
}

bool Joint::setJointMaxGeneralizedForce(const std::vector<double>& maxForce)
{
    // if (maxForce.size() != this->dofs()) {
    //     LOG(ERROR) << "Wrong number of elements (joint_dofs=" + std::to_string(this->dofs()) + ")";
    //     return false;
    // }
    //
    // pImpl->jointMaxGeneralizedForce = maxForce;
    // return true;

    if (maxForce.size() != this->dofs()) {
        LOG(ERROR) << "Wrong number of elements (joint_dofs=" + std::to_string(this->dofs()) + ")";
        return false;

    }
    return pImpl->monopod_sdk->set_max_torque_target(maxForce[0], pImpl->monopodSdkIndex);

}

scenario::core::PID Joint::pid() const
{
    // get PID of low level controller here
    auto data_op = pImpl->monopod_sdk->get_pid(pImpl->monopodSdkIndex);
    if(data_op.has_value()){
        auto data = data_op.value();
        return scenario::core::PID(data.p, data.i, data.d);
          // Set to default value here.
    }else{
        return scenario::core::PID();
    }
}

bool Joint::setPID(const scenario::core::PID& pid)
{
    // Set PID of lowlevel controller here
    return pImpl->monopod_sdk->set_pid(pid.p, pid.i, pid.d, pImpl->monopodSdkIndex);
}

// scenario::core::JointLimit Joint::jointPositionLimit() const
// {
//     core::JointLimit jointLimit(this->dofs());
//
//     // switch (this->type()) {
//     //     case core::JointType::Revolute:
//     //     case core::JointType::Prismatic: {
//     //         sdf::JointAxis& axis = utils::getExistingComponentData< //
//     //             ignition::gazebo::components::JointAxis>(m_ecm, m_entity);
//     //         jointLimit.min[0] = axis.Lower();
//     //         jointLimit.max[0] = axis.Upper();
//     //         break;
//     //     }
//     //     case core::JointType::Fixed:
//     //         sWarning << "Fixed joints do not have DOFs, limits are not defined"
//     //                  << std::endl;
//     //         break;
//     //     case core::JointType::Invalid:
//     //     case core::JointType::Ball:
//     //         sWarning << "Type of Joint '" << this->name() << "' has no limits"
//     //                  << std::endl;
//     //         break;
//     // }
//
//     return jointLimit;
// }
//
// scenario::core::JointLimit Joint::jointVelocityLimit() const
// {
//     core::JointLimit jointLimit(this->dofs());
//
//     // switch (this->type()) {
//     //     case core::JointType::Revolute:
//     //     case core::JointType::Prismatic: {
//     //         sdf::JointAxis& axis = utils::getExistingComponentData< //
//     //             ignition::gazebo::components::JointAxis>(m_ecm, m_entity);
//     //         jointLimit.min[0] = -axis.MaxVelocity();
//     //         jointLimit.max[0] = axis.MaxVelocity();
//     //         break;
//     //     }
//     //     case core::JointType::Fixed:
//     //         sWarning << "Fixed joints do not have DOFs, limits are not defined"
//     //                  << std::endl;
//     //         break;
//     //     case core::JointType::Invalid:
//     //     case core::JointType::Ball:
//     //         sWarning << "Type of Joint '" << this->name() << "' has no limits"
//     //                  << std::endl;
//     //         break;
//     // }
//
//     return jointLimit;
// }
//
// bool Joint::setJointVelocityLimit(const std::vector<double>& maxVelocity)
// {
//     if (maxVelocity.size() != this->dofs()) {
//         sError << "Wrong number of elements (joint_dofs=" << this->dofs() << ")"
//                << std::endl;
//         return false;
//     }
//
//     // switch (this->type()) {
//     //     case core::JointType::Revolute: {
//     //         sdf::JointAxis& axis = utils::getExistingComponentData< //
//     //             ignition::gazebo::components::JointAxis>(m_ecm, m_entity);
//     //         axis.SetMaxVelocity(maxVelocity[0]);
//     //         return true;
//     //     }
//     //     case core::JointType::Ball:
//     //     case core::JointType::Prismatic:
//     //     case core::JointType::Fixed:
//     //     case core::JointType::Invalid:
//     //         sWarning << "Fixed and Invalid joints have no friction defined."
//     //                  << std::endl;
//     //         return false;
//     // }
//
//     return false;
// }
