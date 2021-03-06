SET( DEFINE
)
SET( INCLUDE
)
SET( LINK
)

if(MSVC)
	add_definitions(/wd4244 /wd4267 /wd4018 /wd4355 /wd4800 /wd4251 /wd4996 /wd4146 /wd4305)
endif()

if(XCODE)
	add_definitions(-DHAVE_PTHREAD)
endif()

create_project(STATIC DEFINE INCLUDE LINK)

if(TARGET ${PROJECT_NAME})
	#message("Running ${CMAKE_CURRENT_SOURCE_DIR}/CMakelists.txt")
	
	# Find the protobuf compiler first
	if(MSVC)
		file(GLOB_RECURSE ProtoCompiler ${${PROJECT_NAME}_SOURCE_DIR}/protoc.exe)
	else()
		file(GLOB_RECURSE ProtoCompiler ${${PROJECT_NAME}_SOURCE_DIR}/protoc)
	endif()

	# Loop thru all projects and compile .proto
	foreach(projBinDir ${CMAKE_ALL_PROJECT_DIRS})
		file(GLOB_RECURSE protoFiles ${projBinDir}/*.proto)
		if(protoFiles)
			#message("${projBinDir} has: ${protoFiles}")
			get_folder_name(${projBinDir} projName)
			file(GLOB_RECURSE existingProtoSourceList ${${projName}_BINARY_DIR}/*.pb.cc ${${projName}_BINARY_DIR}/*.pb.h)

			#build new proto stamp
			set(newProtoStamp "")
			foreach(newFile ${protoFiles})
				file(TIMESTAMP ${newFile} newStamp)
				list(APPEND newProtoStamp "${newFile}, ${newStamp}")
				#message("newfile: ${newFile}, ${newStamp}")
			endforeach()

			#read old stamp
			if(EXISTS "${${projName}_BINARY_DIR}/proto.stamp")
				file(READ "${${projName}_BINARY_DIR}/proto.stamp" oldProtoStamp)
			else()
				message("Proto stamp not found: ${${projName}_BINARY_DIR}/proto.stamp")
			endif()

			if (NOT "${oldProtoStamp}" STREQUAL "${newProtoStamp}")
				#message("oldProtoStamp: ${oldProtoStamp}")
				#message("newProtoStamp: ${newProtoStamp}")
			#if (true AND ProtoCompiler)
				message("${PROJECT_NAME}: Detected changes to .proto files in ${projName}. Recompiling...")

				foreach(PROTO_FILE ${protoFiles})
					get_filename_component(PROTO_FILE_DIR ${PROTO_FILE} DIRECTORY)
					get_filename_component(PROTO_FILE_NAME ${PROTO_FILE} NAME_WE)

					string(LENGTH ${${projName}_SOURCE_DIR} projSrcDirStrSize)
					#math(EXPR projSrcDirStrSize "${projSrcDirStrSize}")
					string(SUBSTRING ${PROTO_FILE_DIR} ${projSrcDirStrSize} -1 protoFileDirSubStr)
					set(protoFileDestDir "${${projName}_BINARY_DIR}${protoFileDirSubStr}")
					#message("PROTO_FILE_DIR: ${PROTO_FILE_DIR}")
					#message("${projName}_SOURCE_DIR: ${${projName}_SOURCE_DIR}")
					#message("projSrcDirStrSize: ${projSrcDirStrSize}")
					#message("protoFileDirSubStr: ${protoFileDirSubStr}")

					message("removing")
					#file(REMOVE_RECURSE "${${PROJECT_NAME}_BINARY_DIR}/${PROTO_FILE_NAME}.pb.cc")
					#file(REMOVE_RECURSE "${${PROJECT_NAME}_BINARY_DIR}/${PROTO_FILE_NAME}.pb.h")
					file(MAKE_DIRECTORY ${protoFileDestDir})

					execute_process(
						COMMAND ${ProtoCompiler}
						-I=${PROTO_FILE_DIR}
						--cpp_out=${protoFileDestDir}
						${PROTO_FILE}
					)
				endforeach()
				
				
				file(WRITE ${${projName}_BINARY_DIR}/proto.stamp "${newProtoStamp}")
			endif()

			set(newProtoSourceList "")
			foreach(PROTO_FILE ${protoFiles})
				#message(status "Add source....;${protoFileDestDir}/${PROTO_FILE_NAME}.pb.cc")
				get_filename_component(PROTO_FILE_NAME ${PROTO_FILE} NAME_WE)
				get_filename_component(PROTO_FILE_DIR ${PROTO_FILE} DIRECTORY)
				string(LENGTH ${${projName}_SOURCE_DIR} projSrcDirStrSize)
				string(SUBSTRING ${PROTO_FILE_DIR} ${projSrcDirStrSize} -1 protoFileDirSubStr)
				set(protoFileDestDir "${${projName}_BINARY_DIR}${protoFileDirSubStr}")
				list(APPEND newProtoSourceList "${protoFileDestDir}/${PROTO_FILE_NAME}.pb.cc")
				list(APPEND newProtoSourceList "${protoFileDestDir}/${PROTO_FILE_NAME}.pb.h")
			endforeach()

			#delete useless proto source files
			foreach(newProtoSource ${newProtoSourceList})
				list(REMOVE_ITEM existingProtoSourceList "${newProtoSource}")
			endforeach()
			if(existingProtoSourceList)
				file(REMOVE ${existingProtoSourceList})
			endif()
		endif()


		
	endforeach()

	add_custom_target(
		${PROJECT_NAME}_UpdateProto
		DEPENDS always_rebuild
	)

	add_custom_command(
		OUTPUT always_rebuild
		COMMAND ${CMAKE_COMMAND} -E echo
	)

	add_custom_command(
		VERBATIM
		TARGET ${PROJECT_NAME}_UpdateProto
		PRE_BUILD
		COMMAND
			${CMAKE_COMMAND}
			-DbuildDir=${CMAKE_BINARY_DIR}
			-DprojBuildDir=${${PROJECT_NAME}_BINARY_DIR}
			-DprojDir=${${PROJECT_NAME}_SOURCE_DIR}
			-DslnDir=${CMAKE_SOURCE_DIR}
			-Dgenerator=${CMAKE_GENERATOR}
			-P ${${PROJECT_NAME}_SOURCE_DIR}/UpdateProto.cmake
		#COMMAND ${CMAKE_COMMAND} -E touch "D:/Github/ServerTest/Build/3rdParty/libprotobuf/CMakeFiles/generate.stamp"
		#COMMAND ${CMAKE_COMMAND} -E touch "D:/Github/ServerTest/Build/CMakeFiles/generate.stamp"
		COMMENT "Checking Proto Files..."
		#COMMAND ${CMAKE_COMMAND} -E echo "ZERO_CHECKZERO_CHECKZERO_CHECKZERO_CHECKZERO_CHECKZERO_CHECKZERO_CHECKZERO_CHECKZERO_CHECKZERO_CHECK "
	)	

	add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_UpdateProto)

	if( MSVC )
		SET_PROPERTY(GLOBAL PROPERTY USE_FOLDERS ON)
		SET_PROPERTY(TARGET ${PROJECT_NAME}_UpdateProto		PROPERTY FOLDER CMakePredefinedTargets)
	endif()

endif()