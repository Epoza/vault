# Test out the application using docker

# If you have docker installed first build the image
# docker build -t vault-test .
# The . specifies the current directory (make sure you are in the Vault directory)

# Run the container
# docker run --privileged -it vault-test

# Lastly, execute the install script
#./install.sh

FROM ubuntu

WORKDIR /Vault

# Setup
RUN apt update -y
RUN apt install -y g++ cmake gocryptfs



# Copy files
COPY . .

# Give install script execute permissions
RUN chmod +x install.sh
