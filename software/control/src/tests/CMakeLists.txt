add_executable(testQPReactiveRecovery QPReactiveRecoveryTest.cpp)
target_link_libraries(testQPReactiveRecovery QPReactiveRecoveryPlan)
pods_install_executables(testQPReactiveRecovery)

add_executable(testAtlasFallDetector atlasFallDetectorTest.cpp)
target_link_libraries(testAtlasFallDetector AtlasFallDetector)
pods_install_executables(testAtlasFallDetector)


# test for ik constraints which also publishes the result to LCM:
add_executable(drc-testIKMoreConstraintsLCM testIKMoreConstraintsLCM.cpp)
target_link_libraries(drc-testIKMoreConstraintsLCM boost_thread boost_system boost_filesystem boost_signals)
pods_use_pkg_config_packages(drc-testIKMoreConstraintsLCM drake-rbm drake-ik lcm eigen3)
pods_install_executables(drc-testIKMoreConstraintsLCM )

# Equivalent test for Valkyrie
add_executable(drc-testIKMoreConstraintsLCMValkyrie testIKMoreConstraintsLCMValkyrie.cpp)
target_link_libraries(drc-testIKMoreConstraintsLCMValkyrie boost_thread boost_system boost_filesystem boost_signals)
pods_use_pkg_config_packages(drc-testIKMoreConstraintsLCMValkyrie drake-rbm drake-ik lcm eigen3)
pods_install_executables(drc-testIKMoreConstraintsLCMValkyrie )

# Test for collision between points and robot model using drake and bullet
add_executable(drc-testDrakeBulletCollisions testDrakeBulletCollisions.cpp)
target_link_libraries(drc-testDrakeBulletCollisions boost_system)
pods_use_pkg_config_packages(drc-testDrakeBulletCollisions lcm 
  bot2-lcmgl-client drake-rbm drake-geometry-util)
pods_install_executables(drc-testDrakeBulletCollisions)


# Demo application to do gaze tracking with the Valkyrie head and SCS control
add_executable(drc-gaze-tracking-demo gazeTrackingDemo.cpp)
target_link_libraries(drc-gaze-tracking-demo boost_thread boost_system boost_filesystem boost_signals)
pods_use_pkg_config_packages(drc-gaze-tracking-demo drake-rbm drake-ik lcm eigen3)
pods_install_executables(drc-gaze-tracking-demo )