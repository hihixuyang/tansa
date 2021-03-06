#include <tansa/routine.h>
#include <tansa/model.h>

#include <algorithm>


/*
	Need to determine based on a model whether or not a trajectory is possible

	-> Numberically step through the trajectory at control rate and ensure that the given snap/jerk can be generated by the model at the current state
	-> It would be nice to abstract it such that air drag is also considered to allow for top speed considerations
*/

namespace tansa {

bool FeasibilityChecker::check(Routine &r) {

	auto actions = r.actions;
	auto homes = r.homes;


	auto floatComp = [](double a, double b) -> bool { return fabs(a-b) < 0.1; };
	auto pointComp = [](Point a, Point b) -> bool { return fabs((a-b).norm()) < 0.1; };
	for (unsigned j = 0; j < actions.size(); j++) {
		TrajectoryState startPoint;
		startPoint.position = homes[j];
		startPoint.velocity = Vector3d(0,0,0);
		startPoint.acceleration = Vector3d(0,0,0);

		double startTime = 0.0;
		//Sort actions for each drone based on start time
		std::sort(actions[j].begin(), actions[j].end(),
		[](Action *const &lhs, Action *const &rhs) { return lhs->GetStartTime() < rhs->GetStartTime(); });
		for (unsigned i = 0; i < actions[j].size(); i++) {
			Action *a = actions[j][i];

			if(a->IsLightAction()) {
				continue;
			}


			double sTime = a->GetStartTime();
			double eTime = a->GetEndTime();
			//Check temporal continuity
			if (!floatComp(sTime, startTime)) {
				errors.push_back({
					a->line,
					("Time Discontinuity for Drone: " + std::to_string(j) + " with start time: " +
					std::to_string(sTime) + ". Last command ended at : " + std::to_string(startTime))
				});
			}

			startTime = eTime;

			//Check spatial continuity
			auto ma = dynamic_cast<MotionAction *>(a);
			TrajectoryState actionStart = ma->GetPath()->evaluate(ma->GetStartTime());
			if (!pointComp(actionStart.position, startPoint.position)) {
				errors.push_back({
					ma->line,
					("Spatial Discontinuity for Drone: " + std::to_string(j) + ". Jumping from point: " +
					"[" + std::to_string(startPoint.position.x()) + " " + std::to_string(startPoint.position.y()) + " " +
					std::to_string(startPoint.position.z()) + "]" +
					" to point: " "[" + std::to_string(actionStart.position.x()) + " " +
					std::to_string(actionStart.position.y()) + " " + std::to_string(actionStart.position.z()) + "]" + "\n"
					+ "at start time: " + std::to_string(sTime))

				});
			}

			if(!pointComp(actionStart.velocity, startPoint.velocity) || !pointComp(actionStart.acceleration, startPoint.acceleration)) {
				errors.push_back({
					a->line,
					"Velocity/Acceleration discontinuity: Missing transition?"
				});
			}

			this->check(ma->GetPath(), ma->line);


			startPoint = ma->GetPath()->evaluate(ma->GetEndTime());
		}
	}


	// Checking interdrone distance and making sure that no drones are on top of each othere
	/*
	double t = 0;
	vector<int> indices(0, actions.size());
	for(double t = 0.0; true; t += 0.1) {

		vector<Vector3d> positions; // Positions

		for(int i = 0; i < actions.size(); i++) {


			// TODO:

		}



	}
	*/

	std::sort(errors.begin(), errors.end());

	for(const auto& s : errors) {
		std::cout << "Line " << s.line << ": " << s.text << std::endl;
	}
	return (errors.size() == 0);

}


bool FeasibilityChecker::check(Trajectory::Ptr traj, int line) {

	bool gravityFlag = false, velocityFlag = false, accelFlag = false;

	for(double t = traj->startTime(); t < traj->endTime(); t += 0.01) {

		TrajectoryState s = traj->evaluate(t);


		// Warn when going faster than 2.5 m/s
		if(!velocityFlag && s.velocity.norm() > 2.5) {
			velocityFlag = true;
			errors.push_back({ line, "Going too fast!" });
		}


		Vector3d accel_total = s.acceleration + Vector3d(0, 0, GRAVITY_MS);

		// Typically (unless inverted) cannot accelerate downward faster than gravity
		if(!gravityFlag && accel_total.z() < 0) {
			gravityFlag = true;
			errors.push_back({ line, "Accelerating down too fast!" });
		}

		// Motors working too hard
		if(!accelFlag && accel_total.norm() > 2*GRAVITY_MS*0.8) {
			accelFlag = true;
			errors.push_back({ line, "Accelerating too fast!" });
		}

	}


	// TODO: Change this
	return false;

}


}
