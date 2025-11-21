print("resnet_train")

import torch
import torchvision.models as models
import torch.nn as nn
import torch.optim as optim
import time

print("starting up")

device = torch.device("cpu")
model = models.resnet50().to(device)

# Get the current number of threads (optional)
# current_threads = torch.get_num_threads()
# print(f"Current number of threads: {current_threads}")

# # Set the desired number of threads
# desired_threads = 4  # Example: set to 4 threads
# torch.set_num_threads(desired_threads)

# # Verify the new number of threads (optional)
# new_threads = torch.get_num_threads()
# print(f"New number of threads: {new_threads}")

# Synthetic data
batch_size = 128
dummy_input = torch.randn(batch_size, 3, 224, 224).to(device)
dummy_target = torch.randint(0, 1000, (batch_size,)).to(device)

criterion = nn.CrossEntropyLoss()
optimizer = optim.SGD(model.parameters(), lr=0.01)

# Config
epochs = 2
iters_per_epoch = 2

model.train()
for epoch in range(1, epochs + 1):
    start = time.time()
    for i in range(iters_per_epoch):
        print(f"Iteration {i + 1}/{iters_per_epoch}")
        optimizer.zero_grad()
        output = model(dummy_input)
        loss = criterion(output, dummy_target)
        loss.backward()
        optimizer.step()
    end = time.time()
    images_per_sec = (iters_per_epoch * batch_size) / (end - start)
    print(f"Epoch {epoch}: {images_per_sec:.2f} images/sec")
