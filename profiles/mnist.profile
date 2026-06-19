profile_id=mnist-default
artifact_id=mnist-onnx-v1
backend=onnxruntime-cpu
metrics_file_template=/tmp/tear-metric-{artifact_id}-{profile_id}

adaptation.keep_current_profile=allowed
adaptation.request_high_accuracy_profile=allowed
adaptation.reject_input=allowed
