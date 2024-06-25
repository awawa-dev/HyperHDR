# Copyright 2023 @ awawa-dev
if (NOT WIN32)	
	if (QMQTT_LIBRARIES AND QMQTT_INCLUDE_DIRS)
		set(QMQTT_FOUND TRUE)
	else ()
		find_path(QMQTT_INCLUDE_DIRS NAMES qmqtt.h)
		find_library(QMQTT_LIBRARIES NAMES qmqtt libqmqtt)

		if (QMQTT_INCLUDE_DIRS AND QMQTT_LIBRARIES)
			set(QMQTT_FOUND TRUE)
			add_library(qmqtt SHARED IMPORTED GLOBAL)
			set_target_properties(qmqtt PROPERTIES IMPORTED_LOCATION "${QMQTT_LIBRARIES}")
			set_target_properties(qmqtt PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${QMQTT_INCLUDE_DIRS}")
		endif()

		mark_as_advanced(MQTT_INCLUDE_DIRS QMQTT_LIBRARIES)
	endif()
endif()
                                                              
