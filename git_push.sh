# ----- ----- ----- -----
# git_push.sh
# Do not distribute or modify
# Author: DragonTaki (https://github.com/DragonTaki)
# Create Date: 2025/11/22
# Update Date: 2025/11/22
# Version: v1.0
# ----- ----- ----- -----

#!/bin/bash

# ----- Config -----
GIT_REMOTE_NAME="origin"
GIT_REMOTE_URL="https://github.com/DragonTaki/Elevator-Exam.git"
MAIN_BRANCH="main"

# Exit immediately on error
set -e

# Initialize Git if not already initialized
if [ ! -d ".git" ]; then
  echo "🔧 Initializing Git repository..."
  git init
  git remote add "$GIT_REMOTE_NAME" "$GIT_REMOTE_URL"
else
  echo "✅ Git already initialized. Skipping init."
fi

# Check remote version before pushing
echo "🔍 Checking remote version on GitHub..."
git fetch "$GIT_REMOTE_NAME" "$MAIN_BRANCH"

LOCAL_HASH=$(git rev-parse HEAD)
REMOTE_HASH=$(git rev-parse "$GIT_REMOTE_NAME/$MAIN_BRANCH")

if [ "$LOCAL_HASH" != "$REMOTE_HASH" ]; then
  echo "⚠️ Remote branch '$MAIN_BRANCH' has new commits."
  echo "🛑 Please pull the latest changes before pushing:"
  echo ""
  echo "   git pull --rebase $GIT_REMOTE_NAME $MAIN_BRANCH"
  echo ""
  echo "❌ Push aborted to prevent overwrite."
  exit 1
else
  echo "✅ Local branch is up to date with remote."
fi

# Stage all files and commit
echo "📦 Staging all files and committing..."
git add .
git status
git commit -m "Commit from Git Bash" || echo "ℹ️ No changes to commit."

# Prompt before pushing
read -p "❓ Are you sure you want to push to '$MAIN_BRANCH'? (y/n): " confirm
if [[ "$confirm" =~ ^[Yy]$ ]]; then
    echo "🚀 Pushing to GitHub..."
    git push "$GIT_REMOTE_NAME" "$MAIN_BRANCH"
    echo "✅ Done! Your code is now correctly uploaded to GitHub."
else
    echo "❌ Push cancelled."
fi
