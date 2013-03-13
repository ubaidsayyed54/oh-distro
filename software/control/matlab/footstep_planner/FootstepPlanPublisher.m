classdef FootstepPlanPublisher
	properties
		lc
		channel
	end

	methods
		function obj = FootstepPlanPublisher(channel)
			obj.channel = channel;
			obj.lc = lcm.lcm.LCM.getSingleton();
		end

		function publish(obj, X)
			obj.lc.publish(obj.channel, FootstepPlanPublisher.encodeFootstepPlan(X));
		end

	end

	methods(Static)
		function msg = encodeFootstepGoal(X)
			sizecheck(X, [15, 1]);
		  msg = drc.footstep_goal_t();
		  msg.utime = 0;
		  msg.robot_name = 'atlas';
		  msg.pos = drc.position_3d_t();
		  trans = drc.vector_3d_t();
		  trans.x = X(1);
		  trans.y = X(2);
		  trans.z = X(3);
		  rot = drc.quaternion_t();
		  q = FootstepPlanPublisher.quatnormalize(angle2quat(X(4), X(5), X(6), 'XYZ'));
		  rot.x = q(1);
		  rot.y = q(2);
		  rot.z = q(3);
		  rot.w = q(4);
		  msg.pos.translation = trans;
		  msg.pos.rotation = rot;
		  msg.step_time = int64(X(7) * 1000000);
		  msg.id = int32(X(8));
		  msg.fixed_x = X(9);
		  msg.fixed_y = X(10);
		  msg.fixed_z = X(11);
		  msg.fixed_roll = X(12);
		  msg.fixed_pitch = X(13);
		  msg.fixed_yaw = X(14);
		  msg.is_right_foot = X(15);
		end

		function qout = quatnormalize(q)
			norm = sqrt(q(1)^2 + q(2)^2 + q(3)^2 + q(4)^2);
			qout = q ./ repmat(norm, 1, 4);
		end

		function msg = encodeFootstepPlan(X)
		  robot_name = 'atlas';
		  msg = drc.footstep_plan_t();
		  msg.robot_name = robot_name;
		  msg.num_steps = size(X, 2);

		  goals = javaArray('drc.footstep_goal_t', size(X, 2));
		  for j = 1:length(X(1,:))
		  	new_goal = FootstepPlanPublisher.encodeFootstepGoal(X(:,j));
		    goals(j) = new_goal;
		  end
		  msg.footstep_goals = goals;
		end
	end
end
