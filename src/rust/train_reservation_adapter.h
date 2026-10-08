/*
	* This file is part of OpenTTD.
	* OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
	* OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
	* See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <https://www.gnu.org/licenses/old-licenses/gpl-2.0>.
	*/

/** @file train_reservation_adapter.h Named ordinary callback/reentry boundary. */
static OpenTTDTrainReservationStep TrainReservationRun(uint32_t kind, const Train *train, uint64_t a, uint64_t b, uint64_t c)
{
	static const OpenTTDTrainReservationLeaves leaves{TrainReservationObserve, TrainReservationLeaf, TrainReservationOwner, TrainReservationFollow, TrainReservationOrigin};
	TrainReservationContext context;
	auto destroy = [](OpenTTDTrainReservationTask *task) { openttd_rust_train_reservation_destroy(task); };
	std::unique_ptr<OpenTTDTrainReservationTask, decltype(destroy)> task(openttd_rust_train_reservation_new(kind, train->index.base(), a, b, c, &leaves, &context), destroy);
	OpenTTDTrainReservationStep reply{};
	for (;;) {
		OpenTTDTrainReservationStep step = openttd_rust_train_reservation_step(task.get(), reply);
		if (step.action == 0) return step;
		Train *v = Train::Get(VehicleID(step.id));
		reply = step;
		switch (step.action) {
			case 1: { // YAPF station randomisation/animation callbacks.
			 bool found = true;
			 PBSTileInfo dest;
			 TileIndex final_dest = INVALID_TILE;
			 Track track = YapfTrainChooseTrack(v, TileIndex(step.tile), static_cast<DiagDirection>(step.dir), static_cast<TrackBits>(step.tracks), found, step.reserve != 0, &dest, step.got ? &final_dest : nullptr);
			 reply.value = track; reply.found = found; reply.tile = dest.tile.base();
			 reply.td = dest.trackdir; reply.okay = dest.okay; reply.final_dest = final_dest.base();
			 break;
			}
			case 2: // YAPF station randomisation/animation callbacks.
			 reply.value = YapfTrainFindNearestSafeTile(v, TileIndex(step.tile), static_cast<Trackdir>(step.td), step.reserve != 0); break;
			case 4: reply.value = ProcessOrders(v); break;
			case 6: reply.value = UpdateOrderDest(v, v->GetOrder(static_cast<VehicleOrderID>(step.value)), 0, true); break;
			case 8: reply.value = TryReserveRailTrack(TileIndex(step.tile), static_cast<Track>(step.td)); break;
			default: NOT_REACHED();
		}
	}
}
