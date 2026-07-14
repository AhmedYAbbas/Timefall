#pragma once

#include "Timefall/Core/Core.h"
#include "Timefall/Scene/Components.h"

#include <string>

namespace Timefall
{
	namespace Utils
	{
		static std::string RigidBody2DBodyTypeToString(Rigidbody2DComponent::BodyType bodyType)
		{
			switch (bodyType)
			{
				case Rigidbody2DComponent::BodyType::Static: return "Static";
				case Rigidbody2DComponent::BodyType::Dynamic: return "Dynamic";
				case Rigidbody2DComponent::BodyType::Kinematic: return "Kinematic";
			}

			TF_CORE_ASSERT(false, "Unknown body type");
			return {};
		}

		static Rigidbody2DComponent::BodyType RigidBody2DBodyTypeFromString(const std::string& bodyTypeString)
		{
			if (bodyTypeString == "Static")
				return Rigidbody2DComponent::BodyType::Static;
			if (bodyTypeString == "Dynamic")
				return Rigidbody2DComponent::BodyType::Dynamic;
			if (bodyTypeString == "Kinematic")
				return Rigidbody2DComponent::BodyType::Kinematic;

			TF_CORE_ASSERT(false, "Unknown body type");
			return Rigidbody2DComponent::BodyType::Static;
		}
	}
}