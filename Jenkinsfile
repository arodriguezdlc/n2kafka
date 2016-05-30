
gitlabCommitStatus {
    node ('docker') {
        stage 'Build'
        docker.image('resources.redborder.lan:5000/rb-cbuilds-n2kafka:1.1').inside {
            git branch: 'continuous-integration', credentialsId: 'jenkins_id', url: 'git@gitlab.redborder.lan:dfernandez.ext/n2kafka.git'
            sh './configure --disable-optimization'
            sh 'make'
            stage 'Unit Tests'
            sh 'make tests'
        }
    }
    
}
