function [xtraj, info, simVars, statVars] = exploringRRT(options, rng_seed)
      
  %%TIMING
  global IKTimes
  global collisionTimes
  global collisionFails
  global tree
  tree = 5;
  IKTimes = [];
  collisionTimes = [];
  collisionFails = []; %1: SE3.isCollisionFree
                       %2: JointSpaceTree.isCollisionFree
                       %3: OptimalTree.checkEdge
  
  if nargin < 1 || isempty(options), options = struct(); end
  
  w = warning('off','Drake:RigidBodyManipulator:UnsupportedContactPoints');
  warning('off','Drake:RigidBodyManipulator:UnsupportedVelocityLimits');
  warning('off','Drake:RigidBodyManipulator:UnsupportedJointLimits');
  if ~isfield(options,'goal_bias'), options.goal_bias = 0.5; end;
  if ~isfield(options,'n_smoothing_passes'), options.n_smoothing_passes = 10; end;
  if ~isfield(options,'planning_mode'), options.planning_mode = 'multiRRT'; end;
  if ~isfield(options,'visualize'), options.visualize = true; end;
  if ~isfield(options,'scene'), options.scene = 6; end;
  if ~isfield(options,'model'), options.model = 'val'; end;
  if ~isfield(options,'convex_hull'), options.convex_hull = false; end;
  if ~isfield(options,'graspingHand'), options.graspingHand = 'right'; end;
  if ~isfield(options,'costType'), options.costType = 'length'; end;
  if ~isfield(options,'firstFeasibleTraj'), options.firstFeasibleTraj = false; end;
  if ~isfield(options,'robot'), options.robot = []; end;
  if ~isfield(options,'nTrees'), options.nTrees = 4; end;
  if ~isfield(options,'goalObject'), options.goalObject = 2; end;
  
  
  options.floating = true;
  options.terrain = MyRigidBodyFlatTerrain(); %Changed to a smaller terrain to avoid visualization problem when zooming
  options.joint_v_max = 15*pi/180;
  options.viewer = 'NullVisualizer';
  
  if isempty(options.robot)
    r = Scenes.generateScene(options);
  else
    r = options.robot;
  end
  
  if nargin > 1
    rng(rng_seed);
  end
  rndSeed = rng;
  save lastRndg.mat rndSeed
  
  lFoot = Scenes.getLeftFoot(options, r);
  rFoot = Scenes.getRightFoot(options, r);
  
  g_hand = Scenes.getGraspingHand(options, r);
  point_in_link_frame = Scenes.getPointInLinkFrame(options);
  
  q_nom = Scenes.getFP(options.model, r);
  warning(w);
  
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  if options.visualize
    v = r.constructVisualizer();
    v.draw(0, q_nom);
  end
  
  %Set IK options
  ik_seed_pose = q_nom;
  ik_nominal_pose = q_nom;
  cost = Point(r.getPositionFrame(),10);
  for i = r.getNumBodies():-1:1
    if all(r.getBody(i).parent > 0) && all(r.getBody(r.getBody(i).parent).position_num > 0)
      cost(r.getBody(r.getBody(i).parent).position_num) = ...
        cost(r.getBody(r.getBody(i).parent).position_num) + cost(r.getBody(i).position_num);
    end
  end
  cost = cost/min(cost);
  Q = diag(cost);
  ikoptions = IKoptions(r);
  ikoptions = ikoptions.setMajorIterationsLimit(100);
  ikoptions = ikoptions.setQ(Q);
  ikoptions = ikoptions.setMajorOptimalityTolerance(1e-3);
  
  %Set start pose constraints and compute starting configuration
  startPoseConstraints = [Scenes.fixedFeetConstraints(options, r),...
                          {Scenes.addQuasiStaticConstraint(options, r),...
                          Scenes.nonGraspingHandDistanceConstraint(options, r, 0.4)}];
  [q_start, info, infeasible_constraint] = inverseKin(r, ik_seed_pose, ik_nominal_pose, startPoseConstraints{:}, ikoptions);
  if options.visualize
    v.draw(0, q_start);
    %     drawLinkFrame(r, g_hand, q_start, 'Grasping Hand Start');
    %     drawLinkFrame(r, Scenes.getNonGraspingHand(options, r), q_start, 'Non Grasping Hand Start');
    %     drawLinkFrame(r, r.findLinkId('l_ufarm'), q_start, 'Forearm Start');
  end
  
  %Set end pose constraints and compute end configuration
  goalConstraints = Scenes.addGoalConstraint(options, r);
  endPoseConstraints = [startPoseConstraints, goalConstraints];
  switch options.scene
    case 2
%       if strcmp(options.model, 'val')
%         endPoseConstraints = [endPoseConstraints, {Scenes.nonGraspingHandPositionConstraint(options, r)}];
%       else
        endPoseConstraints = [endPoseConstraints, {Scenes.graspingForearmAlignConstraint(options, r)}];
%       end
    case 3
      endPoseConstraints = [endPoseConstraints, {Scenes.pelvisOffsetConstraint(options,r)}];%, Scenes.nonGraspingHandPositionConstraint(options, r)}];
  end
  [q_end, info, infeasible_constraint] = inverseKin(r, ik_seed_pose, ik_nominal_pose, endPoseConstraints{:}, ikoptions);
  if options.visualize
    v.draw(0, q_end);
    %     drawLinkFrame(r, g_hand, q_end, 'Grasping Hand End');
    %     drawLinkFrame(r, r.findLinkId('l_ufarm'), q_end, 'Forearm End');
  end
  
  %Create RRTs
  
  % internal parts removed as they created self collisions:
  if strcmp(options.model, 'val')
    LeftHipRotator = r.findLinkId('LeftHipRotator');
    RightHipRotator = r.findLinkId('RightHipRotator');
    LeftHipAdductor = r.findLinkId('LeftHipAdductor');
    RightHipAdductor = r.findLinkId('RightHipAdductor');
    LowerNeckExtensor = r.findLinkId('LowerNeckExtensor');
    
    % these shouldn't be culled from the link list but its easier too do than
    % fixing meshes now:
    RightForearm = r.findLinkId('RightForearm'); % main welding link
    LeftForearm = r.findLinkId('LeftForearm'); % main welding link
    Head = r.findLinkId('Head'); % main welding link
    inactive_collision_bodies = [lFoot,rFoot];%, LeftHipAdductor, RightHipAdductor,  LeftHipRotator, RightHipRotator, LowerNeckExtensor];%, LeftForearm, RightForearm, Head];
  else
    inactive_collision_bodies = [lFoot,rFoot];
  end
  
  min_distance = 0.01;
  active_collision_options.body_idx = setdiff(1:r.getNumBodies(),inactive_collision_bodies);
  options.display_after_every = 1;
  if any(strcmp(options.planning_mode, {'rrt*', 'multiRRT'}))
    TA = OptimalMotionPlanningTree(r, g_hand, point_in_link_frame);
    TA.steerFactor = 0.1;
  else
    TA = TaskSpaceMotionPlanningTree(r, g_hand, point_in_link_frame);
  end
  TA  = TA.setMinDistance(min_distance);
  TA  = TA.setOrientationWeight(1);
  TA.max_edge_length = 0.05;
  TA.max_length_between_constraint_checks = TA.max_edge_length;
  TA.angle_tol = 10*pi/180;
  TA.position_tol = 1e-3;
  TA.trees{TA.cspace_idx}.active_collision_options.body_idx = setdiff(1:r.getNumBodies(), inactive_collision_bodies);
  TA.trees{TA.cspace_idx}.ikoptions = ikoptions;
  
  kinsol = r.doKinematics(q_start);
  xyz_quat_start = r.forwardKin(kinsol,g_hand,point_in_link_frame,2);
  kinsol = r.doKinematics(q_end);
  xyz_quat_goal = r.forwardKin(kinsol,g_hand,point_in_link_frame,2);
  x_start = [xyz_quat_start;q_start];
  x_goal = [xyz_quat_goal;q_end];
  xyz_box_edge_length = 2;
  xyz_min = min(xyz_quat_start(1:3),xyz_quat_goal(1:3)) - xyz_box_edge_length/2;
  xyz_max = max(xyz_quat_start(1:3),xyz_quat_goal(1:3)) + xyz_box_edge_length/2;
  
  %Naive reduction of the task space
  xyz_min = [x_start(1) - 0.2; min([x_start(2), x_goal(2)]) - 0.5; 0];
  xyz_max = [x_goal(1) + 0.2; max([x_start(2), x_goal(2)]) + 0.5; xyz_max(3)];
  
  TA = TA.setTranslationSamplingBounds(xyz_min, xyz_max);
  TA = TA.addKinematicConstraint(startPoseConstraints{:});
  TA = TA.setNominalConfiguration(q_nom);
  
  TA = TA.compile();
  TB = TA;
  TC = TA;
  TD = TA;
  
  switch options.model
    case 'val'
      qNominalC = Scenes.getFP('valkyrie_fp_rHand_up', r);
      qNominalD = Scenes.getFP('valkyrie_fp_rHand_up_right', r);
    case 'v4'
      qNominalC = Scenes.getFP('atlas_fp_rHand_up', r);
      qNominalD = Scenes.getFP('atlas_fp_rHand_up_right', r);
  end
  
  kinsol = r.doKinematics(qNominalC);
  EEpose = r.forwardKin(kinsol, Scenes.getGraspingHand(options, r), Scenes.getPointInLinkFrame(options), 2);
  constraints = [startPoseConstraints, Scenes.generateEEConstraints(r, options, EEpose)];
  [qStartC, info, infeasible_constraint] = inverseKin(r, qNominalC, qNominalC, constraints{:}, ikoptions);
  if options.visualize
    v.draw(0, qStartC);
  end
  kinsol = r.doKinematics(qStartC);
  xyz_quat_start = r.forwardKin(kinsol,g_hand,point_in_link_frame,2);
  xStartC = [xyz_quat_start; qStartC];
   
  kinsol = r.doKinematics(qNominalD);
  EEpose = r.forwardKin(kinsol, Scenes.getGraspingHand(options, r), Scenes.getPointInLinkFrame(options), 2);
  constraints = [startPoseConstraints, Scenes.generateEEConstraints(r, options, EEpose)];
  [qStartD, info, infeasible_constraint] = inverseKin(r, qNominalD, qNominalD, constraints{:}, ikoptions);
  if options.visualize
    v.draw(0, qStartD);
  end
  kinsol = r.doKinematics(qStartD);
  xyz_quat_start = r.forwardKin(kinsol,g_hand,point_in_link_frame,2);
  xStartD = [xyz_quat_start; qStartD];
  assert(TA.checkConstraints(x_start))
  assert(TB.checkConstraints(x_goal))
  assert(TB.checkConstraints(xStartD))
  assert(TB.checkConstraints(xStartD))
  
  if options.visualize
    v.draw(0, q_start);
  end
  
  % n_ee_poses_tried = 1;
  %sample_prog = InverseKinematics(r,ik_nominal_pose,base_constraints{:},collision_constraint);
  % sample_prog = InverseKinematics(r,ik_nominal_pose,base_constraints{:});
  % sample_prog = sample_prog.setQ(0.1*ikoptions.Q);
  % sample_prog = sample_prog.setSolverOptions('snopt','MajorIterationsLimit',ikoptions.SNOPT_IterationsLimit);
  % sample_prog.setSolverOptions('snopt','MajorFeasibilityTolerance',ikoptions.SNOPT_MajorFeasibilityTolerance);
  % sample_prog.setSolverOptions('snopt','MajorOptimalityTolerance',1e-3);
  TA = TA.setLCMGL('TA',[1,0,0]);
  %TA.TB = TA.TB.setLCMGL('TA.TB',[1,0,0]);
  TB = TB.setLCMGL('TB',[0,0,1]);
  TC = TC.setLCMGL('TC',[1,0,1]);
  TD = TD.setLCMGL('TD',[0,1,1]);
  rrt_timer = tic;
  %display('About to plan ...')
  switch options.planning_mode
    case 'rrt'
      [TA, path_ids_A, info] = TA.rrt(x_start, x_goal, TA, options);
    case 'rrt_connect'
      %display('Running RRTConnect')
      [TA, path_ids_A, info, TB] = TA.rrtConnect(x_start, x_goal, TB, options);
    case 'rrt*'
      [TA, info, cost, q_path] = TA.rrtStar(x_start, x_goal, options);
    case 'multiRRT'
      cm = CapabilityMap([fileparts(which('exploringRRT')) '/CapabilityMap/capabilityMap.mat']);
      switch options.nTrees
        case 4
          multiTree = MultipleTreeProblem([TA, TB, TC, TD], [x_start, x_goal, xStartC, xStartD], goalConstraints, 'capabilityMap', cm, 'graspingHand', options.graspingHand);
        case 3
          multiTree = MultipleTreeProblem([TA, TB, TC], [x_start, x_goal, xStartC], goalConstraints, 'capabilityMap', cm, 'graspingHand', options.graspingHand);
        case 2
          multiTree = MultipleTreeProblem([TA, TB], [x_start, x_goal], goalConstraints, 'capabilityMap', cm, 'graspingHand', options.graspingHand);
      end
      [multiTree, info, cost, q_path, times] = multiTree.rrt(x_start, x_goal, options);
  end
  rrt_time = toc(rrt_timer);
  if options.visualize
    fprintf('  Timing:\n');
    fprintf('    RRT:       %5.2f s\n', rrt_time);
  end
  
  
  if (info == Info.SUCCESS)
    if ~any(strcmp(options.planning_mode, {'rrt*', 'multiRRT'}))
      T_smooth = TA;
      simVars.TConnected = T_smooth;
      path_ids_C = path_ids_A;
      % interp_weight determines how much consideration is given to joint
      % space distance during smoothing:
      %  * 0 - end-effector distance only
      %  * 1 - joint-space distance only
      T_smooth.interp_weight = 0.5;
      q_idx = TA.idx{TA.cspace_idx};
      if (options.n_smoothing_passes > 0)
        smoothing_timer = tic;
        T_smooth = T_smooth.setLCMGL('T_smooth', TA.line_color);
        [T_smooth, id_last] = T_smooth.recursiveConnectSmoothing(path_ids_A, options.n_smoothing_passes, options.visualize);
        path_ids_A = T_smooth.getPathToVertex(id_last);
        smoothing_time = toc(smoothing_timer);
        if options.visualize
          fprintf('    Smoothing: %5.2f s\n', smoothing_time);
          drawTree(TA);
          drawTree(TB);
          drawPath(T_smooth, path_ids_A);
        end
      end
      q_path = extractPath(T_smooth, path_ids_A);
    end
    path_length = size(q_path,2);
    
    % Scale timing to obey joint velocity limits
    % Create initial spline
    q_traj = PPTrajectory(pchip(linspace(0, 1, path_length), q_path(TA.idx{TA.cspace_idx},:)));
    t = linspace(0, 1, 10*path_length);
    q_path = eval(q_traj, t);
    
    % Determine max joint velocity at midpoint of  each segment
    t_mid = mean([t(2:end); t(1:end-1)],1);
    v_mid = max(abs(q_traj.fnder().eval(t_mid)), [], 1);
    
    % Adjust durations to keep velocity below max
    t_scaled = [0, cumsum(diff(t).*v_mid/mean(options.joint_v_max))];
    tf = t_scaled(end);
    
    % Warp time to give gradual acceleration/deceleration
    t_scaled = tf*(-real(acos(2*t_scaled/tf-1)+pi)/2);
    [t_scaled, idx_unique] = unique(t_scaled,'stable');
    
    rState = r.getStateFrame();
    xtraj = PPTrajectory(pchip(t_scaled,[q_path(:,idx_unique); zeros(r.getNumVelocities(),numel(t_scaled))]));
    xtraj = xtraj.setOutputFrame(r.getStateFrame());
    
    %Output structures
    switch options.planning_mode
      case 'rrt_connect'
        simVars.TA = TA;
        simVars.TB = TB;
        simVars.T_smooth = T_smooth;
        simVars.path_ids_A = path_ids_A;
        simVars.path_ids_C = path_ids_C;
        simVars.rndSeed = rndSeed;
        statVars.rrtTime = rrt_time;
        statVars.smoothingTime = smoothing_time;
        statVars.totTime = rrt_time + smoothing_time;
        statVars.TAn = TA.n;
        statVars.TBn = TB.n;
        statVars.TCn = simVars.TConnected.n;
        statVars.TSn = T_smooth.n;
        statVars.info = info;
        q_path = extractPath(T_smooth, path_ids_A);
        TSlengths = q_path(1:3,2:end)-q_path(1:3, 1:end-1);
        statVars.TSlength = sum(diag(sqrt(TSlengths'*TSlengths)));
        q_pathC = extractPath(simVars.TConnected, path_ids_C);
        TClengths = q_pathC(1:3,2:end)-q_pathC(1:3, 1:end-1);
        statVars.TClength = sum(diag(sqrt(TClengths'*TClengths)));
        statVars.options = rmfield(options, {'robot', 'terrain'});
        %distance
      case 'rrt*'
        simVars.TA = TA;        
        simVars.info = info;
        statVars.time = rrt_time;
        statVars.numberOfVertices = TA.n;
        statVars.info = info;
        statVars.cost = cost;
        statVars.options = rmfield(options, {'robot', 'terrain'});
      case 'multiRRT'       
        simVars.info = info;
        statVars.time = rrt_time;
        statVars.reachingTime = times.reaching;
        statVars.improvingTime = times.improving;
        statVars.checkingTime = times.checking;
        if options.visualize
          fprintf('Reaching Time: %.2f\nImproving Time: %.2f\nShortcut Time: %.2f\nrebuild Time: %.2f\ntotal Time: %.2f\n',...
            times.reaching, times.improving, times.shortcut, times.rebuild, rrt_time);
        end
%         statVars.numberOfVertices = TA.n;
        statVars.info = info;
        statVars.cost = cost;
        statVars.rndSeed = rndSeed; 
        statVars.options = rmfield(options, {'robot', 'terrain'});
        statVars.collisionTimes = collisionTimes;
        statVars.IKTimes = IKTimes;
        statVars.collisionFails = collisionFails;
    end
    
    if options.visualize
      v.playback(xtraj);
    end
  else
    xtraj = [];
    v = [];
    simVars.info = info;
    statVars.info = info;
    statVars.rndSeed = rndSeed; 
    fprintf('Failed to find a solution (%s)\n', info.getStatus())
  end
  
end



