def clean_action(env, targets, action):

    if env.GetOption('clean'):
        env.Execute(action(targets,env))

def generate(env):
    env.AddMethod(clean_action, 'CleanAction')

def exists(env):
    return True 
