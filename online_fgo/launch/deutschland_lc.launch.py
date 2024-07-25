import os
import yaml
from ament_index_python.packages import get_package_share_directory
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, Command
from launch_ros.actions import Node

from launch import LaunchDescription


def generate_launch_description():
    logger = LaunchConfiguration("log_level")

    config_common_path = LaunchConfiguration('config_common_path')
    default_config_common = os.path.join(
        get_package_share_directory('online_fgo'),
        'config/deutschland_lc',
        'common.yaml'
    )

    default_config_integrator = os.path.join(
        get_package_share_directory('online_fgo'),
        'config/deutschland_lc',
        'integrator.yaml'
    )

    default_config_optimizer = os.path.join(
        get_package_share_directory('online_fgo'),
        'config/deutschland_lc',
        'optimizer.yaml'
    )

    declare_config_common_path_cmd = DeclareLaunchArgument(
        'config_common_path',
        default_value=default_config_common,
        description='CommonParameters')

    declare_config_integrtor_path_cmd = DeclareLaunchArgument(
        'config_common_path',
        default_value=default_config_integrator,
        description='IntegratorParameters')

    declare_config_optimizer_path_cmd = DeclareLaunchArgument(
        'config_common_path',
        default_value=default_config_optimizer,
        description='OptimizerParameters')

    default_config_sensor_parameters = os.path.join(
        get_package_share_directory('online_fgo'),
        'config/deutschland_lc',
        'sensor_parameters.yaml'
    )
    declare_config_sensor_parameters_path_cmd = DeclareLaunchArgument(
        'config_common_path',
        default_value=default_config_sensor_parameters,
        description='SensorParameters')

    online_fgo_node = Node(
        package='online_fgo',
        executable='online_fgo_node',
        name="online_fgo",
        namespace="deutschland",
        output='screen',
        emulate_tty=True,
        # prefix=['gdb -ex run --args'],
        # arguments=['--ros-args', '--log-level', logger],
        parameters=[
            config_common_path,
            default_config_common,
            default_config_integrator,
            default_config_optimizer,
            default_config_sensor_parameters,
            {

            }
            # Overriding
            # {
            # }
        ]  # ,
        # remapping=[
        #
        # ]
    )


    # Define LaunchDescription variable and return it
    ld = LaunchDescription()

    ld.add_action(DeclareLaunchArgument(
        "log_level",
        default_value=["debug"],
        description="Logging level"))
    ld.add_action(declare_config_common_path_cmd)
    ld.add_action(declare_config_integrtor_path_cmd)
    ld.add_action(declare_config_optimizer_path_cmd)
    ld.add_action(declare_config_sensor_parameters_path_cmd)
    ld.add_action(online_fgo_node)
    # ld.add_action(plot_node)

    return ld
