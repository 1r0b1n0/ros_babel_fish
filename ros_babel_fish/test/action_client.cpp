//
// Created by Stefan Fabian on 03.03.20.
//

#include "message_comparison.h"
#include <ros_babel_fish/actionlib/babel_fish_action.h>
#include <ros_babel_fish/babel_fish.h>
#include <ros_babel_fish_test_msgs/SimpleTestAction.h>

using namespace ros_babel_fish;
using namespace ros_babel_fish_test_msgs;

TEST( ActionClientTest, simpleActionClient )
{
  auto provider = std::make_shared<MessageOnlyDescriptionProvider>();
  BabelFish fish( provider );
  provider->registerMessageByDefinition( ros::message_traits::datatype<SimpleTestAction>(),
                                         ros::message_traits::definition<SimpleTestAction>());
  MessageDescription::ConstPtr goal_description = provider->getMessageDescription(
    ros::message_traits::datatype<SimpleTestActionGoal>());
  actionlib::SimpleActionClient<BabelFishAction> client( goal_description, "simple" );
  if ( !client.waitForServer( ros::Duration( 10 )))
    FAIL() << "ActionServer did not start within 10 seconds!";
  ASSERT_TRUE( client.isServerConnected());

  // This goal should succeed
  Message::Ptr goal = fish.createMessage( "ros_babel_fish_test_msgs/SimpleTestGoal" );
  (*goal)["goal"] = 5;
  BabelFishMessage::ConstPtr goal_msg = fish.translateMessage( goal );
  actionlib::SimpleClientGoalState state = client.sendGoalAndWait( *goal_msg, ros::Duration( 10 ));
  EXPECT_EQ( state, actionlib::SimpleClientGoalState::SUCCEEDED );
  BabelFishMessage::ConstPtr result = client.getResult();
  TranslatedMessage::ConstPtr translated = fish.translateMessage( result );
  EXPECT_EQ((*translated->translated_message)["result"].value<int32_t>(), 4 );

  // This goal should abort after 10
  goal = fish.createMessage( "ros_babel_fish_test_msgs/SimpleTestGoal" );
  (*goal)["goal"] = 20;
  goal_msg = fish.translateMessage( goal );
  std::vector<int> feedback_values;
  client.sendGoal( *goal_msg, {}, {}, boost::function<void( const BabelFishMessage::ConstPtr & )>(
    [ & ]( const BabelFishMessage::ConstPtr &feedback )
    {
      TranslatedMessage::ConstPtr translated = fish.translateMessage( feedback );
      feedback_values.push_back((*translated->translated_message)["feedback"].value<int32_t>());
    } ));
  ASSERT_EQ( client.getState(), actionlib::SimpleClientGoalState::PENDING );
  if ( !client.waitForResult( ros::Duration( 10 )))
  {
    FAIL() << "ActionServer did not finish in 10 seconds!";
  }
  ASSERT_EQ( feedback_values.size(), 10U );
  for ( int i = 0; i < 10; ++i )
  {
    if ( feedback_values[i] != i ) FAIL() << "Feedback at " << i << " should be " << i << "!";
  }
  EXPECT_EQ( client.getState(), actionlib::SimpleClientGoalState::ABORTED );
  result = client.getResult();
  translated = fish.translateMessage( result );
  EXPECT_EQ((*translated->translated_message)["result"].value<int32_t>(), 10 );

  // This goal should be preempted
  goal = fish.createMessage( "ros_babel_fish_test_msgs/SimpleTestGoal" );
  (*goal)["goal"] = 1000;
  goal_msg = fish.translateMessage( goal );
  feedback_values.clear();
  client.sendGoal( *goal_msg, {}, {}, boost::function<void( const BabelFishMessage::ConstPtr & )>(
    [ & ]( const BabelFishMessage::ConstPtr &feedback )
    {
      TranslatedMessage::ConstPtr translated = fish.translateMessage( feedback );
      feedback_values.push_back((*translated->translated_message)["feedback"].value<int32_t>());
    } ));
  usleep( 500000 ); // Sleep for 500ms
  client.cancelGoal();
  if ( !client.waitForResult( ros::Duration( 1 )))
    FAIL() << "ActionServer did not preempt in 1 second!";
  int last_feedback = 0;
  for ( size_t i = 0; i < feedback_values.size(); ++i )
  {
    if ( feedback_values[i] != int( i )) FAIL() << "Feedback at " << i << " should be " << i << "!";
    last_feedback = feedback_values[i];
  }
  EXPECT_EQ( client.getState(), actionlib::SimpleClientGoalState::PREEMPTED );
  result = client.getResult();
  translated = fish.translateMessage( result );
  EXPECT_EQ((*translated->translated_message)["result"].value<int32_t>(), last_feedback );
}

int main( int argc, char **argv )
{
  testing::InitGoogleTest( &argc, argv );
  ros::init( argc, argv, "test_action_client" );
  ros::NodeHandle nh;
  return RUN_ALL_TESTS();
}
