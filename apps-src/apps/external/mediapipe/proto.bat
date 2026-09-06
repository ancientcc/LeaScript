set SCRIPTS=c:\ddksample\apps-src\scripts
set GOOGLE=C:\ddksample\apps-src\apps\external\protobuf\src
set CARTOGRAPHER=C:\ddksample\apps-src\apps\external\mediapipe
set TENSORFLOW=C:\ddksample\apps-src\apps\external\tensorflow


set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\calculators\audio
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. mfcc_mel_calculators.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. rational_factor_resample_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. spectrogram_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. stabilized_log_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. time_series_framer_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. two_tap_fir_filter_calculator.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\calculators\core
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. bypass_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. clip_vector_size_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. concatenate_vector_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. constant_side_packet_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. dequantize_byte_array_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. flow_limiter_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. gate_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. get_vector_item_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. graph_profile_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. packet_cloner_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. packet_resampler_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. packet_thinner_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. quantize_float_vector_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. sequence_shift_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. split_vector_calculator.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\calculators\image
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. bilateral_filter_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. feature_detector_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. image_clone_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. image_cropping_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. image_transformation_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. mask_overlay_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. opencv_encoded_image_to_image_frame_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. opencv_image_encoder_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. recolor_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. rotation_mode.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. scale_image_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. segmentation_smoothing_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. set_alpha_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. warp_affine_calculator.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\calculators\internal
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. callback_packet_calculator.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\calculators\tensor
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. audio_to_tensor_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. bert_preprocessor_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. feedback_tensors_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. image_to_tensor_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. image_to_tensor_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. inference_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. landmarks_to_tensor_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. regex_preprocessor_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tensor_converter_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tensor_to_joints_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tensors_readback_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tensors_to_audio_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tensors_to_classification_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tensors_to_detections_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tensors_to_floats_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tensors_to_landmarks_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tensors_to_segmentation_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. vector_to_tensor_calculator.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\calculators\tensorflow
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. graph_tensors_packet_generator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. image_frame_to_tensor_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. lapped_tensor_buffer_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. matrix_to_tensor_calculator_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. object_detection_tensors_to_detections_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. pack_media_sequence_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensor_squeeze_dimensions_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensor_to_image_frame_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensor_to_matrix_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensor_to_vector_float_calculator_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensor_to_vector_int_calculator_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensor_to_vector_string_calculator_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensorflow_inference_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensorflow_session_from_frozen_graph_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensorflow_session_from_frozen_graph_generator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensorflow_session_from_saved_model_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. tensorflow_session_from_saved_model_generator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. unpack_media_sequence_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. vector_float_to_tensor_calculator_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. vector_int_to_tensor_calculator_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%TENSORFLOW% --proto_path=%CUR_DIR% --cpp_out=. vector_string_to_tensor_calculator_options.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\calculators\tflite
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. ssd_anchors_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tflite_converter_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tflite_custom_op_resolver_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tflite_inference_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tflite_tensors_to_classification_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tflite_tensors_to_detections_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tflite_tensors_to_landmarks_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tflite_tensors_to_segmentation_calculator.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\calculators\util
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. align_hand_to_pose_in_world_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. annotation_overlay_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. association_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. collection_has_min_size_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. combine_joints_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. detection_label_id_to_text_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. detections_to_rects_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. detections_to_render_data_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. face_to_rect_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. filter_detections_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. flat_color_image_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. labels_to_render_data_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. landmark_projection_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. landmarks_refinement_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. landmarks_smoothing_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. landmarks_to_detection_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. landmarks_to_floats_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. landmarks_to_render_data_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. landmarks_transformation_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. latency.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. local_file_contents_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. logic_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. non_max_suppression_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. packet_frequency.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. packet_frequency_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. packet_latency_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. rect_to_render_data_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. rect_to_render_scale_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. rect_transformation_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. refine_landmarks_from_heatmap_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. resource_provider_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. set_joints_visibility_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. thresholding_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. timed_box_list_id_to_label_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. timed_box_list_to_render_data_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. top_k_scores_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. visibility_copy_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. visibility_smoothing_calculator.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\framework
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. calculator_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. calculator_profile.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. graph_runtime_info.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. mediapipe_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. packet_factory.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. packet_generator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. status_handler.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. stream_handler.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. thread_pool_executor.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\framework\deps
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. proto_descriptor.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\framework\formats
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. body_rig.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. classification.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. detection.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. image_format.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. landmark.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. location_data.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. matrix_data.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. rect.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. time_series_header.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\framework\formats\object_detection
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. anchor.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\framework\stream_handler
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. default_input_stream_handler.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. fixed_size_input_stream_handler.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. sync_set_input_stream_handler.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. timestamp_align_input_stream_handler.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\framework\formats\annotation
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. locus.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. rasterization.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\framework\tool
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. calculator_graph_template.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. field_data.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. packet_generator_wrapper_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. source.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. switch_container.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\gpu
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. gl_animation_overlay_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. gl_context_options.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. gl_scaler_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. gl_surface_sink_calculator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. gpu_origin.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. scale_mode.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. gl_surface_sink_calculator.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\util
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. audio_decoder.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. color.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. label_map.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. render_data.proto

set CUR_DIR=C:\ddksample\apps-src\apps\external\mediapipe\mediapipe\util\tracking
cd %CUR_DIR%
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. box_detector.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. box_tracker.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. camera_motion.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. flow_packager.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. frame_selection.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. frame_selection_solution_evaluator.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. motion_analysis.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. motion_estimation.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. motion_models.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. motion_saliency.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. push_pull_filtering.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. region_flow.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. region_flow_computation.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tone_estimation.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tone_models.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tracked_detection_manager_config.proto
%SCRIPTS%\protoc-3.21.8.exe --proto_path=%CARTOGRAPHER% --proto_path=%CUR_DIR% --cpp_out=. tracking.proto